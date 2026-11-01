// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_ble.cpp
// STRATO 6 (packages): ciclo di vita e politica del Bluetooth. La radio sta in vos_drv_ble; le funzioni dei programmi in vos_ble_app.cpp.
// Il Bluetooth non parte mai da solo: lo chiede un programma VesevOS con il permesso ble (bleAppOn) e lo avvia bleTick (ciclo principale),
// cosi la radio non viene inizializzata dal task del programma.
#include "vos_ble_int.h"
#include "../core/vos_eventbus.h"
#include "../core/vos_config.h"
#include "../core/vos_log.h"
#include "../core/vos_i18n.h"
#include "../core/vos_audit.h"
#include "../core/vos_ram.h"
#include "../core/vos_service.h"
#include "../drivers/vos_drv_time.h"
#include "../drivers/vos_drv_heap.h"
#include "../drivers/vos_drv_sys.h"
#include "vos_stats.h"

#if VOS_WITH_BLE
#define BLE_PAUSE_MS  5000UL                         // pausa tra due accensioni
#define BLE_MIN_FREE  85000u                         // RAM libera minima per accendere
#define BLE_MIN_BLOCK 35000u                         // blocco libero piu grande minimo (misurato con altro programma: acceso anche con 47 e 59 KB)
#define BLE_ADV_MS    500                            // intervallo dell'annuncio

namespace vosble {
bool g_conn = false;                                  // un telefono e collegato
VosMutex g_mx;
Phase    g_phase = IDLE;
char     g_owner[BLE_OWNER_MAX + 1] = "";
char     g_name[BLE_NAME_MAX + 1] = "VesevOS";
BleServiceDef g_def;
bool     g_adv = false;
uint32_t g_limMin = 0;
bool     g_endReq = false;
char     g_err[12] = "";
uint32_t g_stopAt = 0;
}
using namespace vosble;

static uint32_t g_until = 0, g_pin = 0, g_wantAt = 0;
static bool     g_lim = false;
static bool   (*g_alive)(const char*) = nullptr;     // il programma proprietario gira ancora?

void bleSetOwnerCheck(bool (*alive)(const char*)) { g_alive = alive; }

void vosble::resetConfig() {
  snprintf(g_name, sizeof g_name, "VesevOS");
  memset(&g_def, 0, sizeof g_def);
  g_adv = false; g_limMin = 0; g_lim = false;
}

bool vosble::claim(const char* owner, ErrBuf& err) {
  if (!owner || !owner[0]) { err = tr("Bluetooth: solo un'app con il permesso ble puo usarlo"); return false; }
  if (g_owner[0] && strcmp(g_owner, owner) != 0) { err.setf(tr("Bluetooth gia in uso dal programma %s"), g_owner); return false; }
  if (!g_owner[0]) { snprintf(g_owner, sizeof g_owner, "%s", owner); resetConfig(); g_endReq = false; g_err[0] = 0; }
  return true;
}

// ---- collegamento (task Bluetooth) ----
void vosble::onLinkEvent(bool up) {
  g_conn = up;
  EventBus::publish("ble.link", "", up ? 1 : 0);
  if (up) statNote(ST_BLE_CONN);
  vlog(up ? "BLE: telefono collegato" : "BLE: telefono scollegato");
}

// ---- accensione e spegnimento ----
static void releaseAll(const char* why, const char* errCode) {
  bool wasUp = (g_phase == UP);
  char who[BLE_OWNER_MAX + 1]; snprintf(who, sizeof who, "%s", g_owner);
  if (wasUp) drvBleEnd();
  queueClear();
  g_phase = IDLE; g_conn = false; g_endReq = false; g_lim = false; g_pin = 0;
  g_owner[0] = 0;
  resetConfig();
  snprintf(g_err, sizeof g_err, "%s", errCode ? errCode : "");
  if (wasUp) {
    g_stopAt = drvMillis(); if (!g_stopAt) g_stopAt = 1;
    ramNote("ble off");
    vlog("BLE: spento (%s), RAM libera %u KB, blocco piu grande %u KB", why, (unsigned)(drvHeapFree() / 1024), (unsigned)(drvHeapMaxBlock() / 1024));
    auditEvent(AUD_INFO, "ble", trf("Bluetooth spento (programma %s)", who).c_str());
  }
}

void bleStop() {
  VosLock lk(g_mx);
  if (g_phase != IDLE) releaseAll("forzato", "");
}

// Controlli prima di accendere: gli stessi per la richiesta (errore subito al programma) e per l'avvio.
static bool precheck(ErrBuf& err) {
  if (cfg.airOn) { err = tr("Modalita aereo attiva: radio spenta"); return false; }
  if (drvHeapFree() < BLE_MIN_FREE) { err = tr("Memoria insufficiente per il Bluetooth: chiudi la pagina web e riprova"); return false; }
  if (g_stopAt && drvMillis() - g_stopAt < BLE_PAUSE_MS) { err.setf(tr("Bluetooth appena spento: attendi %d secondi e riprova"), (int)((BLE_PAUSE_MS - (drvMillis() - g_stopAt)) / 1000 + 1)); return false; }
  if (drvHeapMaxBlock() < BLE_MIN_BLOCK) { err = tr("Memoria troppo frammentata per il Bluetooth: chiudi la pagina web, attendi qualche secondo e riprova"); return false; }
  return true;
}

bool bleAppOn(const char* owner, uint32_t minutes, ErrBuf& err) {
  VosLock lk(g_mx);
  if (minutes > BLE_MINUTES_MAX) { err.setf(tr("Bluetooth: massimo %d minuti"), BLE_MINUTES_MAX); return false; }
  if (!claim(owner, err)) return false;
  if (g_phase != IDLE) { g_limMin = minutes; if (g_phase == UP) { g_lim = minutes > 0; g_until = drvMillis() + minutes * 60000UL; } return true; }   // gia acceso: rinnova il tempo
  if (!precheck(err)) { g_owner[0] = 0; resetConfig(); return false; }
  g_limMin = minutes;
  g_phase = WANT; g_wantAt = drvMillis();                          // lo avvia bleTick
  return true;
}

bool bleAppOff(const char* owner, ErrBuf& err) {
  VosLock lk(g_mx);
  if (!owner || strcmp(g_owner, owner) != 0) { if (!g_owner[0]) return true; err.setf(tr("Bluetooth gia in uso dal programma %s"), g_owner); return false; }
  releaseAll("richiesto dal programma", "");
  return true;
}

void bleAppEnded(const char* owner) {
  VosLock lk(g_mx);
  if (owner && g_owner[0] && strcmp(g_owner, owner) == 0) g_endReq = true;
}

static void startNow() {
  ErrBuf e;
  if (!precheck(e)) {
    if (cfg.airOn || drvHeapFree() < BLE_MIN_FREE) { vlog("BLE: accensione non riuscita: %s", e.c_str()); releaseAll("accensione non riuscita", "ram"); }
    if (drvMillis() - g_wantAt > 30000UL) { vlog("BLE: accensione non riuscita: %s", e.c_str()); releaseAll("accensione non riuscita", "ram"); }   // dopo 30 s si rinuncia
    return;                                                        // pausa o memoria frammentata: si riprova al prossimo giro
  }
  g_pin = 100000 + drvRandom32() % 900000;                          // casuale a ogni accensione, mai fisso
  BleDrvConfig dc;
  memset(&dc, 0, sizeof dc);
  snprintf(dc.name, sizeof dc.name, "%s", g_name);
  dc.passkey = g_pin; dc.advertise = g_adv; dc.advIntervalMs = BLE_ADV_MS;
  dc.svc = g_def; dc.ev = *driverEvents();
  BleDrvErr r = drvBleBegin(dc);
  memset(&dc, 0, sizeof dc);                                        // niente copie del codice nello stack
  if (r != BLE_OK) { vlog("BLE: accensione non riuscita (errore %d)", (int)r); releaseAll("accensione non riuscita", r == BLE_ERR_RAM ? "ram" : "start"); return; }
  g_phase = UP; g_conn = false;
  g_lim = g_limMin > 0; g_until = drvMillis() + g_limMin * 60000UL;
  statNote(ST_BLE_START);
  ramNote("ble on");
  vlog("BLE: acceso dal programma %s%s", g_owner, g_lim ? " (con limite di tempo)" : "");   // il codice di abbinamento non va mai nel registro
  auditEvent(AUD_INFO, "ble", trf("Bluetooth acceso dal programma %s", g_owner).c_str());
}

void bleTick() {
  VosLock lk(g_mx);
  if (g_phase == IDLE) return;
  if (g_endReq || (g_alive && !g_alive(g_owner))) { releaseAll("il programma e finito", ""); return; }   // anche se il programma e stato fermato di forza
  if (g_phase == WANT) { startNow(); return; }
  drvBleTick();
  if (drvBleLocked()) { vlog("BLE: troppi collegamenti senza abbinamento, spento"); releaseAll("troppi tentativi", "locked"); return; }
  if (!drvBleIsUp()) { releaseAll("radio ferma", "start"); return; }
  if (g_lim && (int32_t)(g_until - drvMillis()) <= 0) { releaseAll("tempo scaduto", "timeout"); return; }
}

bool bleRunning() { return g_phase == UP; }
const char* bleStateName() { return g_phase != UP ? "off" : g_conn ? "connected" : "on"; }

void bleStatusLine(char* out, size_t n) {
  VosLock lk(g_mx);
  if (g_phase != UP) {
    if (g_err[0]) snprintf(out, n, "off err=%s", g_err); else snprintf(out, n, "off");
    return;
  }
  long left = g_lim ? (long)((int32_t)(g_until - drvMillis()) / 1000) : -1;
  if (left < 0 && g_lim) left = 0;
  snprintf(out, n, "%s owner=%s left=%ld adv=%s paired=%s name=%s", bleStateName(), g_owner, left, g_adv ? "yes" : "no", drvBleSecure() ? "yes" : "no", g_name);
}

bool blePasskey(char* out, size_t n) {
  VosLock lk(g_mx);
  if (g_phase != UP || n < 7) return false;
  snprintf(out, n, "%06lu", (unsigned long)(g_pin % 1000000UL));
  return true;
}

// ---- servizio nel registro (vos_service): solo stato e spegnimento. Accendere da qui NON e possibile ----
static int      svcState() { return g_phase == UP ? SVC_ON : SVC_OFF; }
static uint32_t svcRam() { return g_phase == UP ? 62000u : 0u; }               // misurato (1.7.38, free detail): ~62 KB con la radio accesa
static bool     svcBegin(ErrBuf& err) { err = tr("Il Bluetooth si accende solo da un programma VesevOS con il permesso ble"); return false; }
static bool     svcJson(HeapBuf& out) { return out.addf("{\"have\":true,\"on\":%s}", g_phase == UP ? "true" : "false"); }
static const ServiceOps BLE_OPS = { "ble", "Bluetooth", "Radio", "ble", ROLE_ADMIN, ROLE_ADMIN,
                                    svcState, svcBegin, bleStop, nullptr, svcJson, nullptr, svcRam };

void bleServiceInit() {
  g_mx.init();
  resetConfig();
  serviceRegister(&BLE_OPS);                         // niente RAM Bluetooth al boot: tutto parte solo su richiesta di un programma
}
#endif
