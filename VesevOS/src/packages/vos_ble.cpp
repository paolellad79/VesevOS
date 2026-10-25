// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_ble.cpp
#include "vos_ble.h"
#include "../core/vos_eventbus.h"
#include "../core/vos_common.h"
#include "../core/vos_config.h"
#include "../net/vos_net.h"
#include "../core/vos_log.h"
#include "../core/vos_i18n.h"
#include "../core/vos_util.h"
#include "../core/vos_heapbuf.h"
#include "../core/vos_strbuf.h"
#include "../core/vos_ram.h"
#include "../core/vos_service.h"
#include "../drivers/vos_drv_ble.h"
#include "vos_ble_cfg.h"
#include "vos_stats.h"
#include "../core/vos_time.h"
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include "../drivers/vos_drv_time.h"
#include "../drivers/vos_drv_heap.h"
#include "../drivers/vos_drv_sys.h"
#include <algorithm>

#if VOS_WITH_BLE
// STRATO 4/2: ciclo di vita del Bluetooth (limiti di memoria, tempo facoltativo, pausa tra due accensioni, codice di accoppiamento,
// stato per pagina e shell). La radio sta in vos_drv_ble; i comandi dal telefono in vos_ble_cfg.

#define BLE_SVC  "5b0a0001-7665-7365-766f-730000000001"
#define BLE_CMD  "5b0a0002-7665-7365-766f-730000000001"
#define BLE_RESP "5b0a0003-7665-7365-766f-730000000001"
#define BLE_PAUSE_MS  5000UL                         // pausa tra due accensioni
#define BLE_MIN_BLOCK 35000u                         // blocco libero piu grande minimo per accendere (misurato: acceso senza problemi anche con 47 e 59 KB)

static bool g_on = false;
static uint32_t g_until = 0, g_pin = 0;
static bool g_lim = false;                           // true = si spegne da solo allo scadere di g_until; false = resta acceso finche non lo si spegne (pagina, shell, app)
static bool g_renamePend = false;                    // il nome della scheda e cambiato: spento e riacceso da solo (appena nessun telefono e collegato)
static uint32_t g_renameMin = 0;                     // limite da rimettere dopo il riavvio per cambio nome (0 = nessuno)
static uint32_t g_totSec = 0;                        // durata del limite in corso (per la barra della Home)
static uint32_t g_limMin = 0;                        // minuti scelti per la PROSSIMA accensione (0 = senza limite)
static volatile bool g_hasCmd = false;
static char g_cmd[201];                              // comando in arrivo dal telefono (max 200 caratteri, come nel driver)
static ErrBuf g_last;                                // ultima risposta al telefono
static char g_hello[96];                             // primo valore annunciato: versione e nome
static char g_name[40];                              // nome Bluetooth = nome della scheda in rete (hostname) al momento dell'accensione
static bool g_conn = false;
static volatile int g_req = 0;                       // richiesta di un driver Berry: 0 nessuna, 1 accendi, 2 spegni (la esegue bleTick)
static bool g_bootPend = false;                      // impostazione "ble" accesa: parte al primo giro del ciclo principale
static uint32_t g_stopAt = 0;                        // quando e stato spento l'ultima volta
// pagina web, rete (modo aereo) e ciclo principale chiamano queste funzioni da task diversi: un solo alla volta
static SemaphoreHandle_t bleMx() { static SemaphoreHandle_t m = xSemaphoreCreateRecursiveMutex(); return m; }
struct BleLock { BleLock() { xSemaphoreTakeRecursive(bleMx(), portMAX_DELAY); } ~BleLock() { xSemaphoreGiveRecursive(bleMx()); } };

// dal driver (task Bluetooth): si mette da parte il comando, si esegue nel ciclo principale
static void onRx(const char* t) { if (g_hasCmd) return; snprintf(g_cmd, sizeof(g_cmd), "%s", t); g_hasCmd = true; }
static void onLink(bool up) {
  g_conn = up;
  EventBus::publish("ble.link", "", up ? 1 : 0);     // telefono collegato / scollegato
  if (up) { statNote(ST_BLE_CONN); vlog("BLE: telefono collegato"); } else vlog("BLE: telefono scollegato");
}

static const char* bleWantedName() { return cfg.hostname.length() ? cfg.hostname.c_str() : "VesevOS"; }   // il nome Bluetooth SEGUE il nome della scheda
bool bleRunning() { return g_on; }
uint32_t bleLeftSec() { return (g_on && g_lim) ? (uint32_t)((g_until - drvMillis()) / 1000) : 0; }
bool bleLimited() { return g_on && g_lim; }
void bleSetLimit(uint32_t minutes) { g_limMin = minutes > 1440 ? 1440 : minutes; }
uint32_t bleTotalSec() { return g_lim ? g_totSec : 0; }
static void armTimer() { g_lim = g_limMin > 0; g_totSec = g_limMin * 60UL; g_until = drvMillis() + g_limMin * 60000UL; }

bool bleStart(ErrBuf& err) {
  BleLock lk;
  if (g_on) { armTimer(); return true; }
  if (cfg.airOn) { err = tr("Modalita aereo attiva: radio spenta"); return false; }
  if (drvHeapFree() < 85000) { err = tr("Memoria insufficiente per il Bluetooth: chiudi la pagina web e riprova"); return false; }
  if (g_stopAt && drvMillis() - g_stopAt < BLE_PAUSE_MS) { err.setf(tr("Bluetooth appena spento: attendi %d secondi e riprova"), (int)((BLE_PAUSE_MS - (drvMillis() - g_stopAt)) / 1000 + 1)); return false; }
  uint32_t big = drvHeapMaxBlock();
  vlog("BLE: prima di accendere, RAM libera %u KB, blocco piu grande %u KB", (unsigned)(drvHeapFree() / 1024), (unsigned)(big / 1024));
  if (big < BLE_MIN_BLOCK) { err = tr("Memoria troppo frammentata per il Bluetooth: chiudi la pagina web, attendi qualche secondo e riprova"); vlog("ATTENZIONE: BLE non acceso, memoria frammentata (blocco piu grande %u KB)", (unsigned)(big / 1024)); return false; }
  g_pin = 100000 + drvRandom32() % 900000;
  snprintf(g_hello, sizeof(g_hello), "VesevOS %s - %s", VOS_VERSION, cfg.hostname.c_str());
  snprintf(g_name, sizeof(g_name), "%s", bleWantedName());
  BleDrvConfig dc = { g_name, BLE_SVC, BLE_CMD, BLE_RESP, g_pin, g_hello, onRx, onLink };
  if (!drvBleBegin(dc)) { err = tr("Memoria insufficiente per il Bluetooth: chiudi la pagina web e riprova"); return false; }
  g_on = true; armTimer(); ramNote("ble on");
  if (g_lim) vlog("BLE: acceso per %u minuti (il codice di accoppiamento si vede solo in pagina o nella shell)", (unsigned)g_limMin);
  else vlog("BLE: acceso, senza limite di tempo (il codice di accoppiamento si vede solo in pagina o nella shell)");   // il registro lo legge anche un ospite: niente codice qui
  return true;
}

static void stopImpl() {
  BleLock lk;
  if (!g_on) return;
  g_on = false; g_conn = false; g_lim = false;
  drvBleEnd();
  g_stopAt = drvMillis(); if (!g_stopAt) g_stopAt = 1; ramNote("ble off");
  vlog("BLE: spento, RAM libera %u KB, blocco piu grande %u KB", (unsigned)(drvHeapFree() / 1024), (unsigned)(drvHeapMaxBlock() / 1024));
}

bool bleEnabled() { return cfg.bleOn; }
const char* bleStateName() { return !g_on ? "off" : g_conn ? "connected" : "on"; }
void bleRequest(bool on) { g_req = on ? 1 : 2; }

bool bleUserOn(uint32_t minutes, ErrBuf& err) {
  bleSetLimit(minutes);
  if (!bleStart(err)) return false;
  if (minutes == 0 && !cfg.bleOn) { cfg.bleOn = true; cfgSave(); }          // senza limite: si ricorda
  return true;
}
void bleUserOff() {
  bleStop();
  if (cfg.bleOn) { cfg.bleOn = false; cfgSave(); }
}

void bleStop() { g_renamePend = false; stopImpl(); }       // spegnimento voluto (pagina, shell, app, scadenza): annulla anche un riavvio in attesa

static void runCmd(const char* c) {
  bool stop = false;
  bleCfgRun(c, stop, g_last);
  drvBleSend(g_last.c_str());
  if (stop) bleStop();
}

void bleTick() {
  BleLock lk;
  if (g_bootPend) {                                                                                // accensione all'avvio (impostazione dell'utente)
    g_bootPend = false; bleSetLimit(0);
    ErrBuf e; if (!bleStart(e)) vlog("BLE: accensione all'avvio non riuscita: %s", e.c_str());
    g_limMin = 0;
  }
  if (g_req) {                                                                                     // richiesta di un driver (non ricordata)
    int rq = g_req; g_req = 0;
    if (rq == 1) { bleSetLimit(0); ErrBuf e; if (!bleStart(e)) vlog("BLE: accensione chiesta da un driver non riuscita: %s", e.c_str()); g_limMin = 0; }
    else bleStop();
  }
  if (g_renamePend && !g_on && g_stopAt && drvMillis() - g_stopAt >= BLE_PAUSE_MS + 300) {      // riaccensione dopo il cambio nome
    g_renamePend = false; bleSetLimit(g_renameMin);
    ErrBuf e; if (!bleStart(e)) vlog("BLE: riaccensione dopo il cambio nome non riuscita: %s", e.c_str());
    g_limMin = 0;
  }
  if (!g_on) return;
  if (!g_conn && !g_renamePend && strcmp(g_name, bleWantedName()) != 0) {                                // nome scheda cambiato: il Bluetooth lo annuncia nuovo
    g_renameMin = g_lim ? (uint32_t)std::max(1L, (long)((g_until - drvMillis()) / 60000UL)) : 0;
    vlog("BLE: il nome della scheda e cambiato, il Bluetooth si riaccende col nome nuovo");
    g_renamePend = true; stopImpl(); return;
  }
  if (g_hasCmd) { char c[sizeof(g_cmd)]; memcpy(c, g_cmd, sizeof(c)); g_hasCmd = false; vlog("BLE: comando ricevuto"); runCmd(c); }
  if (g_lim && (int32_t)(g_until - drvMillis()) <= 0) { vlog("BLE: tempo scaduto"); bleStop(); }
}

bool bleJson(HeapBuf& j) {
  j.addf("{\"have\":true,\"on\":%s,\"pin\":\"", g_on ? "true" : "false");
  if (g_on) j.addUint(g_pin);
  j.add("\",\"name\":\"");
  if (g_on) j.addJson(g_name);
  j.addf("\",\"conn\":%s,\"lim\":%s,\"left\":%lu,\"last\":\"", g_conn ? "true" : "false", bleLimited() ? "true" : "false", (unsigned long)bleLeftSec());
  j.addJson(g_last.c_str(), g_last.length());
  return j.add("\"}");
}
std::string bleText() {
  StrText t(256);
  if (!g_on) { t.buf().add(tr("Bluetooth spento (ble on per accenderlo)")).add('\n'); return t.take(); }
  char p[16]; snprintf(p, sizeof(p), "%06lu", (unsigned long)(g_pin % 1000000UL));
  if (!g_lim) t.buf().addf(tr("Bluetooth acceso: nome %s, codice %s, senza limite di tempo (ble off per spegnerlo)"), g_name, p);
  else t.buf().addf(tr("Bluetooth acceso: nome %s, codice %s, ancora %lu s"), g_name, p, (unsigned long)((g_until - drvMillis()) / 1000));
  t.buf().add('\n');
  return t.take();
}

// ---- contratto dei servizi (vos_service): il Bluetooth e il primo servizio ----
static int      svcState() { return g_on ? SVC_ON : SVC_OFF; }
static uint32_t svcRam() { return g_on ? 62000u : 0u; }                                                   // misurato (1.7.38, free detail): ~62 KB con la radio accesa
static const ServiceOps BLE_OPS = { "ble", "Bluetooth", "Radio", "ble", ROLE_ADMIN, ROLE_ADMIN,
                                    svcState, bleStart, bleStop, nullptr, bleJson, nullptr, svcRam };
// Prova a vuoto al boot, SOLO se l'impostazione "ble" e accesa: dopo il primo ciclo acceso/spento NimBLE lascia 2 piccoli blocchi
// nel mezzo della RAM e il pezzo piu grande scendeva (misurato con Arduino: da 79 a 47 KB). Facendo il ciclo subito, a RAM ancora
// in un pezzo, quei blocchi stanno in fondo. Con il BLE spento (di fabbrica) non si alloca niente al boot.
#ifndef VOS_BLE_PRIME
#define VOS_BLE_PRIME 1
#endif
static void blePrime() {
#if VOS_BLE_PRIME
  uint32_t before = drvHeapMaxBlock();
  if (!drvBlePrime()) return;
  vlog("BLE: memoria preparata al boot, blocco piu grande %u KB prima, %u KB dopo", (unsigned)(before / 1024), (unsigned)(drvHeapMaxBlock() / 1024));
#endif
}
void bleServiceInit() {
  serviceRegister(&BLE_OPS);
  if (!cfg.bleOn) return;                            // spento: niente memoria BLE al boot
  if (cfg.airOn) { vlog("BLE: impostazione accesa ma modo aereo attivo, resta spento"); return; }
  blePrime();
  g_bootPend = true;
}

#endif
