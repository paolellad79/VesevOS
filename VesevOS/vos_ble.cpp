// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_ble.cpp
#include "vos_ble.h"
#include "vos_common.h"
#include "vos_config.h"
#include "vos_region.h"
#include "vos_net.h"
#include "vos_log.h"
#include "vos_i18n.h"
#include "vos_util.h"
#include "vos_stats.h"
#include "vos_time.h"
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#if VOS_WITH_BLE
#include <NimBLEDevice.h>                 // libreria NimBLE-Arduino 2.x: usa molta meno RAM del Bluetooth di Arduino (misurato: quello costava 74 KB)

#define BLE_SVC  "5b0a0001-7665-7365-766f-730000000001"
#define BLE_CMD  "5b0a0002-7665-7365-766f-730000000001"
#define BLE_RESP "5b0a0003-7665-7365-766f-730000000001"
#define BLE_MS   (10UL * 60UL * 1000UL)

static bool g_on = false, g_inited = false;
static uint32_t g_until = 0, g_pin = 0;
static NimBLECharacteristic* g_resp = nullptr;
static volatile bool g_hasCmd = false;
static String g_cmd, g_last;
static bool g_conn = false;
static uint32_t g_stopAt = 0;
// pagina web, rete (modo aereo) e ciclo principale chiamano queste funzioni da task diversi: un solo alla volta
static SemaphoreHandle_t bleMx() { static SemaphoreHandle_t m = xSemaphoreCreateRecursiveMutex(); return m; }
struct BleLock { BleLock() { xSemaphoreTakeRecursive(bleMx(), portMAX_DELAY); } ~BleLock() { xSemaphoreGiveRecursive(bleMx()); } };                         // quando e stato spento l'ultima volta (pausa tra due accensioni)
#define BLE_PAUSE_MS  5000UL
#define BLE_MIN_BLOCK 35000u                         // blocco libero piu grande minimo per accendere (misurato: acceso senza problemi anche con 47 e 59 KB)

class CmdCb : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic* c, NimBLEConnInfo&) override {
    NimBLEAttValue v = c->getValue();
    if (v.length() > 200 || g_hasCmd) return;
    String t; t.reserve(v.length());
    const uint8_t* d = v.data();
    for (size_t i = 0; i < v.length(); i++) t += (char)d[i];
    g_cmd = t; g_hasCmd = true;                       // si esegue nel ciclo principale, non qui
  }
};
class SrvCb : public NimBLEServerCallbacks {
  void onConnect(NimBLEServer*, NimBLEConnInfo&) override { g_conn = true; statNote(ST_BLE_CONN); vlog("BLE: telefono collegato"); }
  void onDisconnect(NimBLEServer*, NimBLEConnInfo&, int) override { g_conn = false; vlog("BLE: telefono scollegato"); if (g_on) NimBLEDevice::startAdvertising(); }
};

bool bleRunning() { return g_on; }
uint32_t bleLeftSec() { return g_on ? (uint32_t)((g_until - millis()) / 1000) : 0; }

bool bleStart(String& err) {
  BleLock lk;
  if (g_on) { g_until = millis() + BLE_MS; return true; }
  if (cfg.airOn) { err = tr("Modalita aereo attiva: radio spenta"); return false; }
  if (ESP.getFreeHeap() < 85000) { err = tr("Memoria insufficiente per il Bluetooth: chiudi la pagina web e riprova"); return false; }
  if (g_stopAt && millis() - g_stopAt < BLE_PAUSE_MS) { err = trf("Bluetooth appena spento: attendi %d secondi e riprova", (int)((BLE_PAUSE_MS - (millis() - g_stopAt)) / 1000 + 1)); return false; }
  uint32_t big = ESP.getMaxAllocHeap();
  vlog("BLE: prima di accendere, RAM libera %u KB, blocco piu grande %u KB", (unsigned)(ESP.getFreeHeap() / 1024), (unsigned)(big / 1024));
  if (big < BLE_MIN_BLOCK) { err = tr("Memoria troppo frammentata per il Bluetooth: chiudi la pagina web, attendi qualche secondo e riprova"); vlog("ATTENZIONE: BLE non acceso, memoria frammentata (blocco piu grande %u KB)", (unsigned)(big / 1024)); return false; }
  NimBLEDevice::init("VesevOS-setup");
  g_pin = 100000 + esp_random() % 900000;
  NimBLEDevice::setSecurityPasskey(g_pin);
  NimBLEDevice::setSecurityIOCap(BLE_HS_IO_DISPLAY_ONLY);     // la scheda MOSTRA il codice, il telefono lo scrive
  NimBLEDevice::setSecurityAuth(false, true, true);           // niente memoria del telefono, protezione MITM, connessione sicura
  NimBLEServer* s = NimBLEDevice::createServer();
  static SrvCb srvCb;                                          // oggetti fissi: con "new" ogni accensione ne lasciava due in RAM
  s->setCallbacks(&srvCb, false);                              // false = la libreria non li cancella (sono statici)
  NimBLEService* svc = s->createService(BLE_SVC);
  NimBLECharacteristic* cmd = svc->createCharacteristic(BLE_CMD, NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_ENC | NIMBLE_PROPERTY::WRITE_AUTHEN);
  static CmdCb cmdCb;
  cmd->setCallbacks(&cmdCb);
  g_resp = svc->createCharacteristic(BLE_RESP, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::READ_ENC | NIMBLE_PROPERTY::READ_AUTHEN | NIMBLE_PROPERTY::NOTIFY);
  { String hi = String("VesevOS ") + VOS_VERSION + " - " + cfg.hostname; g_resp->setValue((const uint8_t*)hi.c_str(), (uint16_t)hi.length()); }
  s->start();
  NimBLEDevice::getAdvertising()->addServiceUUID(BLE_SVC);
  NimBLEDevice::startAdvertising();
  g_inited = true; g_on = true; g_until = millis() + BLE_MS;
  vlog("BLE: acceso per 10 minuti (il codice di accoppiamento si vede solo in pagina o nella shell)");   // il registro lo legge anche un ospite: niente codice qui
  return true;
}

void bleStop() {
  BleLock lk;
  if (!g_on) return;
  g_on = false; g_resp = nullptr; g_conn = false;
  NimBLEDevice::deinit(true);                                  // true = libera anche server e servizi (riparte da zero alla prossima accensione)
  g_stopAt = millis(); if (!g_stopAt) g_stopAt = 1;
  vlog("BLE: spento, RAM libera %u KB, blocco piu grande %u KB", (unsigned)(ESP.getFreeHeap() / 1024), (unsigned)(ESP.getMaxAllocHeap() / 1024));
}

static void reply(const String& r) {
  g_last = r;
  if (g_resp) { g_resp->setValue((const uint8_t*)r.c_str(), (uint16_t)r.length()); g_resp->notify(); }
}

static void run(String c) {
  c.trim();
  String w = c, rest; int sp = c.indexOf(' ');
  if (sp > 0) { w = c.substring(0, sp); rest = c.substring(sp + 1); rest.trim(); }
  cfgSetOrigin("bluetooth");
  if (w == "status") { reply(cfg.hostname + " " + netIpString() + " " + (cfg.country.length() ? cfg.country : String("--"))); return; }
  if (w == "done") { reply("ok"); bleStop(); return; }
  if (w == "wifi") {
    int p = rest.indexOf('|');
    String ssid = p >= 0 ? rest.substring(0, p) : rest, pass = p >= 0 ? rest.substring(p + 1) : String("");
    if (!ssid.length() || ssid.length() > 32 || (pass.length() && (pass.length() < 8 || pass.length() > 63))) { reply(tr("rete o password non valide")); return; }
    cfg.staSsid = ssid; cfg.staPass = pass; cfg.staEnabled = true; cfg.staDhcp = true;
    bool ntpBack = cfgNtpAfterWifi();
    cfgSave(); netReconfigure(); if (ntpBack) timeApply(); reply("ok"); return;
  }
  if (w == "country") {
    rest.toUpperCase();
    if (!regionValid(rest)) { reply(tr("paese sconosciuto")); return; }
    cfg.country = rest; cfgSave(); netReconfigure(); reply("ok"); return;
  }
  if (w == "name") {
    rest.toLowerCase();
    if (!hostnameValid(rest)) { reply(tr("nome non valido")); return; }
    cfg.hostname = rest; cfgSave(); netReconfigure(); reply("ok"); return;
  }
  if (w == "mesh") {
    int p = rest.indexOf(' ');
    int role = p > 0 ? rest.substring(0, p).toInt() : -1;
    String key = p > 0 ? rest.substring(p + 1) : String("");
    key.toLowerCase();
    if (role < 0 || role > 2 || key.length() != 64) { reply(tr("uso: mesh <0|1|2> <chiave di 64 cifre>")); return; }
    cfg.meshRole = role; cfg.meshKey = key; cfgSave(); reply("ok"); return;
  }
  reply(tr("comando sconosciuto (wifi, country, name, mesh, status, done)"));
}

void bleTick() {
  BleLock lk;
  if (!g_on) return;
  if (g_hasCmd) { String c = g_cmd; g_hasCmd = false; vlog("BLE: comando ricevuto"); run(c); }
  if ((int32_t)(g_until - millis()) <= 0) { vlog("BLE: tempo scaduto"); bleStop(); }
}

String bleJson() {
  return "{\"have\":true,\"on\":" + String(g_on ? "true" : "false") + ",\"pin\":\"" + (g_on ? String(g_pin) : String("")) + "\",\"conn\":" + String(g_conn ? "true" : "false") +
         ",\"left\":" + String(g_on ? (unsigned long)((g_until - millis()) / 1000) : 0UL) + ",\"last\":\"" + jsonEscape(g_last) + "\"}";
}
String bleText() {
  if (!g_on) return String(tr("Bluetooth spento (ble on per accenderlo 10 minuti)")) + "\n";
  char p[16]; snprintf(p, sizeof(p), "%06lu", (unsigned long)(g_pin % 1000000UL));
  return trf("Bluetooth acceso: nome VesevOS-setup, codice %s, ancora %lu s", p, (unsigned long)((g_until - millis()) / 1000)) + "\n";
}

#endif
