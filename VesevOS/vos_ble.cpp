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

#if VOS_WITH_BLE
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLESecurity.h>

#define BLE_SVC  "5b0a0001-7665-7365-766f-730000000001"
#define BLE_CMD  "5b0a0002-7665-7365-766f-730000000001"
#define BLE_RESP "5b0a0003-7665-7365-766f-730000000001"
#define BLE_MS   (10UL * 60UL * 1000UL)

static bool g_on = false, g_inited = false;
static uint32_t g_until = 0, g_pin = 0;
static BLECharacteristic* g_resp = nullptr;
static volatile bool g_hasCmd = false;
static String g_cmd, g_last;
static bool g_conn = false;
static uint32_t g_stopAt = 0;                         // quando e stato spento l'ultima volta (pausa tra due accensioni)
#define BLE_PAUSE_MS  5000UL
#define BLE_MIN_BLOCK 45000u                         // blocco libero piu grande minimo per accendere

class CmdCb : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* c) override {
    String v = c->getValue();
    if (v.length() > 200 || g_hasCmd) return;
    g_cmd = v; g_hasCmd = true;                       // si esegue nel ciclo principale, non qui
  }
};
class SrvCb : public BLEServerCallbacks {
  void onConnect(BLEServer*) override { g_conn = true; statNote(ST_BLE_CONN); vlog("BLE: telefono collegato"); }
  void onDisconnect(BLEServer*) override { g_conn = false; vlog("BLE: telefono scollegato"); if (g_on) BLEDevice::startAdvertising(); }
};

bool bleRunning() { return g_on; }
uint32_t bleLeftSec() { return g_on ? (uint32_t)((g_until - millis()) / 1000) : 0; }

bool bleStart(String& err) {
  if (g_on) { g_until = millis() + BLE_MS; return true; }
  if (cfg.airOn) { err = tr("Modalita aereo attiva: radio spenta"); return false; }
  if (ESP.getFreeHeap() < 60000) { err = tr("Memoria insufficiente per il Bluetooth: chiudi la pagina web e riprova"); return false; }
  if (g_stopAt && millis() - g_stopAt < BLE_PAUSE_MS) { err = trf("Bluetooth appena spento: attendi %d secondi e riprova", (int)((BLE_PAUSE_MS - (millis() - g_stopAt)) / 1000 + 1)); return false; }
  uint32_t big = ESP.getMaxAllocHeap();
  vlog("BLE: prima di accendere, RAM libera %u KB, blocco piu grande %u KB", (unsigned)(ESP.getFreeHeap() / 1024), (unsigned)(big / 1024));
  if (big < BLE_MIN_BLOCK) { err = tr("Memoria frammentata: riavvia la scheda per usare il Bluetooth"); vlog("ATTENZIONE: BLE non acceso, memoria frammentata (blocco piu grande %u KB)", (unsigned)(big / 1024)); return false; }
  BLEDevice::init("VesevOS-setup");
  g_pin = 100000 + esp_random() % 900000;
  BLESecurity::setPassKey(true, g_pin);
  BLESecurity::setCapability(ESP_IO_CAP_OUT);                 // la scheda MOSTRA il codice, il telefono lo scrive
  BLESecurity::setAuthenticationMode(false, true, true);      // niente memoria del telefono, protezione MITM, connessione sicura
  BLEServer* s = BLEDevice::createServer();
  static SrvCb srvCb;                                          // oggetti fissi: con "new" ogni accensione ne lasciava due in RAM
  s->setCallbacks(&srvCb);
  BLEService* svc = s->createService(BLE_SVC);
  BLECharacteristic* cmd = svc->createCharacteristic(BLE_CMD, BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_WRITE_ENC | BLECharacteristic::PROPERTY_WRITE_AUTHEN);
  static CmdCb cmdCb;
  cmd->setCallbacks(&cmdCb);
  g_resp = svc->createCharacteristic(BLE_RESP, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_READ_ENC | BLECharacteristic::PROPERTY_READ_AUTHEN | BLECharacteristic::PROPERTY_NOTIFY);
  g_resp->setValue(String("VesevOS ") + VOS_VERSION + " - " + cfg.hostname);
  svc->start();
  BLEDevice::getAdvertising()->addServiceUUID(BLE_SVC);
  BLEDevice::startAdvertising();
  g_inited = true; g_on = true; g_until = millis() + BLE_MS;
  vlog("BLE: acceso per 10 minuti, codice di accoppiamento %06lu", (unsigned long)g_pin);
  return true;
}

void bleStop() {
  if (!g_on) return;
  g_on = false; g_resp = nullptr;
  BLEDevice::deinit(false);
  g_stopAt = millis(); if (!g_stopAt) g_stopAt = 1;
  vlog("BLE: spento, RAM libera %u KB, blocco piu grande %u KB", (unsigned)(ESP.getFreeHeap() / 1024), (unsigned)(ESP.getMaxAllocHeap() / 1024));
}

static void reply(const String& r) {
  g_last = r;
  if (g_resp) { g_resp->setValue(r); g_resp->notify(); }
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
    cfgSave(); netReconfigure(); reply("ok"); return;
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
  char p[8]; snprintf(p, sizeof(p), "%06lu", (unsigned long)g_pin);
  return trf("Bluetooth acceso: nome VesevOS-setup, codice %s, ancora %lu s", p, (unsigned long)((g_until - millis()) / 1000)) + "\n";
}

#else   // senza Bluetooth
bool   bleStart(String& err) { err = tr("Bluetooth non incluso in questo firmware"); return false; }
void   bleStop() {}
bool   bleRunning() { return false; }
uint32_t bleLeftSec() { return 0; }
void   bleTick() {}
String bleJson() { return "{\"have\":false,\"on\":false}"; }
String bleText() { return String(tr("Bluetooth non incluso in questo firmware")) + "\n"; }
#endif
