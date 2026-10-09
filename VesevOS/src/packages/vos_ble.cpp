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
#include "../core/vos_service.h"
#include "../drivers/vos_drv_ble.h"
#include "vos_ble_cfg.h"
#include "vos_stats.h"
#include "../core/vos_time.h"
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

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
static String g_cmd, g_last, g_hello, g_name;      // g_name = nome Bluetooth = nome della scheda in rete (hostname) al momento dell'accensione
static bool g_conn = false;
static uint32_t g_stopAt = 0;                        // quando e stato spento l'ultima volta
// pagina web, rete (modo aereo) e ciclo principale chiamano queste funzioni da task diversi: un solo alla volta
static SemaphoreHandle_t bleMx() { static SemaphoreHandle_t m = xSemaphoreCreateRecursiveMutex(); return m; }
struct BleLock { BleLock() { xSemaphoreTakeRecursive(bleMx(), portMAX_DELAY); } ~BleLock() { xSemaphoreGiveRecursive(bleMx()); } };

// dal driver (task Bluetooth): si mette da parte il comando, si esegue nel ciclo principale
static void onRx(const String& t) { if (g_hasCmd) return; g_cmd = t; g_hasCmd = true; }
static void onLink(bool up) {
  g_conn = up;
  EventBus::publish("ble.link", "", up ? 1 : 0);     // telefono collegato / scollegato
  if (up) { statNote(ST_BLE_CONN); vlog("BLE: telefono collegato"); } else vlog("BLE: telefono scollegato");
}

static String bleWantedName() { return cfg.hostname.length() ? cfg.hostname : String("VesevOS"); }   // il nome Bluetooth SEGUE il nome della scheda
bool bleRunning() { return g_on; }
uint32_t bleLeftSec() { return (g_on && g_lim) ? (uint32_t)((g_until - millis()) / 1000) : 0; }
bool bleLimited() { return g_on && g_lim; }
void bleSetLimit(uint32_t minutes) { g_limMin = minutes > 1440 ? 1440 : minutes; }
uint32_t bleTotalSec() { return g_lim ? g_totSec : 0; }
static void armTimer() { g_lim = g_limMin > 0; g_totSec = g_limMin * 60UL; g_until = millis() + g_limMin * 60000UL; }

bool bleStart(String& err) {
  BleLock lk;
  if (g_on) { armTimer(); return true; }
  if (cfg.airOn) { err = tr("Modalita aereo attiva: radio spenta"); return false; }
  if (ESP.getFreeHeap() < 85000) { err = tr("Memoria insufficiente per il Bluetooth: chiudi la pagina web e riprova"); return false; }
  if (g_stopAt && millis() - g_stopAt < BLE_PAUSE_MS) { err = trf("Bluetooth appena spento: attendi %d secondi e riprova", (int)((BLE_PAUSE_MS - (millis() - g_stopAt)) / 1000 + 1)); return false; }
  uint32_t big = ESP.getMaxAllocHeap();
  vlog("BLE: prima di accendere, RAM libera %u KB, blocco piu grande %u KB", (unsigned)(ESP.getFreeHeap() / 1024), (unsigned)(big / 1024));
  if (big < BLE_MIN_BLOCK) { err = tr("Memoria troppo frammentata per il Bluetooth: chiudi la pagina web, attendi qualche secondo e riprova"); vlog("ATTENZIONE: BLE non acceso, memoria frammentata (blocco piu grande %u KB)", (unsigned)(big / 1024)); return false; }
  g_pin = 100000 + esp_random() % 900000;
  g_hello = String("VesevOS ") + VOS_VERSION + " - " + cfg.hostname;
  g_name = bleWantedName();
  BleDrvConfig dc = { g_name.c_str(), BLE_SVC, BLE_CMD, BLE_RESP, g_pin, g_hello.c_str(), onRx, onLink };
  if (!drvBleBegin(dc)) { err = tr("Memoria insufficiente per il Bluetooth: chiudi la pagina web e riprova"); return false; }
  g_on = true; armTimer();
  if (g_lim) vlog("BLE: acceso per %u minuti (il codice di accoppiamento si vede solo in pagina o nella shell)", (unsigned)g_limMin);
  else vlog("BLE: acceso, senza limite di tempo (il codice di accoppiamento si vede solo in pagina o nella shell)");   // il registro lo legge anche un ospite: niente codice qui
  return true;
}

static void stopImpl() {
  BleLock lk;
  if (!g_on) return;
  g_on = false; g_conn = false; g_lim = false;
  drvBleEnd();
  g_stopAt = millis(); if (!g_stopAt) g_stopAt = 1;
  vlog("BLE: spento, RAM libera %u KB, blocco piu grande %u KB", (unsigned)(ESP.getFreeHeap() / 1024), (unsigned)(ESP.getMaxAllocHeap() / 1024));
}

void bleStop() { g_renamePend = false; stopImpl(); }       // spegnimento voluto (pagina, shell, app, scadenza): annulla anche un riavvio in attesa

static void runCmd(const String& c) {
  bool stop = false;
  g_last = bleCfgRun(c, stop);
  drvBleSend(g_last);
  if (stop) bleStop();
}

void bleTick() {
  BleLock lk;
  if (g_renamePend && !g_on && g_stopAt && millis() - g_stopAt >= BLE_PAUSE_MS + 300) {      // riaccensione dopo il cambio nome
    g_renamePend = false; bleSetLimit(g_renameMin);
    String e; if (!bleStart(e)) vlog("BLE: riaccensione dopo il cambio nome non riuscita: %s", e.c_str());
    g_limMin = 0;
  }
  if (!g_on) return;
  if (!g_conn && !g_renamePend && g_name != bleWantedName()) {                                // nome scheda cambiato: il Bluetooth lo annuncia nuovo
    g_renameMin = g_lim ? (uint32_t)max(1L, (long)((g_until - millis()) / 60000UL)) : 0;
    vlog("BLE: il nome della scheda e cambiato, il Bluetooth si riaccende col nome nuovo");
    g_renamePend = true; stopImpl(); return;
  }
  if (g_hasCmd) { String c = g_cmd; g_hasCmd = false; vlog("BLE: comando ricevuto"); runCmd(c); }
  if (g_lim && (int32_t)(g_until - millis()) <= 0) { vlog("BLE: tempo scaduto"); bleStop(); }
}

String bleJson() {
  return "{\"have\":true,\"on\":" + String(g_on ? "true" : "false") + ",\"pin\":\"" + (g_on ? String(g_pin) : String("")) + "\",\"name\":\"" + (g_on ? jsonEscape(g_name) : String("")) + "\",\"conn\":" + String(g_conn ? "true" : "false") +
         ",\"lim\":" + String(bleLimited() ? "true" : "false") + ",\"left\":" + String((unsigned long)bleLeftSec()) + ",\"last\":\"" + jsonEscape(g_last) + "\"}";
}
String bleText() {
  if (!g_on) return String(tr("Bluetooth spento (ble on per accenderlo)")) + "\n";
  char p[16]; snprintf(p, sizeof(p), "%06lu", (unsigned long)(g_pin % 1000000UL));
  if (!g_lim) return trf("Bluetooth acceso: nome %s, codice %s, senza limite di tempo (ble off per spegnerlo)", g_name.c_str(), p) + "\n";
  return trf("Bluetooth acceso: nome %s, codice %s, ancora %lu s", g_name.c_str(), p, (unsigned long)((g_until - millis()) / 1000)) + "\n";
}

// ---- contratto dei servizi (vos_service): il Bluetooth e il primo servizio ----
static int      svcState() { return g_on ? SVC_ON : SVC_OFF; }
static uint32_t svcRam() { return g_on ? 62000u : 0u; }                                                   // misurato (1.7.38, free detail): ~62 KB con la radio accesa
static const ServiceOps BLE_OPS = { "ble", "Bluetooth", "Radio", "ble", ROLE_ADMIN, ROLE_ADMIN,
                                    svcState, bleStart, bleStop, nullptr, bleJson, nullptr, svcRam };
// Prova a vuoto al boot (VOS_BLE_PRIME = 1): dopo il primo ciclo acceso/spento NimBLE lascia 2 piccoli blocchi nel mezzo della RAM
// e il pezzo piu grande scendeva da 79 a 47 KB (misurato). Facendo il ciclo subito, a RAM ancora in un pezzo, quei blocchi stanno in fondo.
#ifndef VOS_BLE_PRIME
#define VOS_BLE_PRIME 1
#endif
static void blePrime() {
#if VOS_BLE_PRIME
  if (cfg.airOn) return;
  uint32_t before = ESP.getMaxAllocHeap();
  if (!drvBlePrime()) return;
  vlog("BLE: memoria preparata al boot, blocco piu grande %u KB prima, %u KB dopo", (unsigned)(before / 1024), (unsigned)(ESP.getMaxAllocHeap() / 1024));
#endif
}
void bleServiceInit() { serviceRegister(&BLE_OPS); blePrime(); }

#endif
