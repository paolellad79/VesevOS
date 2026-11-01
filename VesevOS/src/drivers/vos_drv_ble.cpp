// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_drv_ble.cpp (strato 1: driver Bluetooth, vedi vos_drv_ble.h)
#include "vos_drv_ble.h"

#if VOS_WITH_BLE
#include <string.h>
#include <stdlib.h>
#include <NimBLEDevice.h>                 // esp-nimble-cpp 2.x (Apache-2.0, componente ESP-IDF senza Arduino)
#include "vos_drv_time.h"

#define NO_CONN          0xFFFF
#define PAIR_WAIT_MS     30000UL          // tempo per abbinarsi
#define IDLE_MS          120000UL         // collegamento fermo oltre questo tempo: chiuso
#define MAX_FAILED       3                // collegamenti senza abbinamento prima del blocco
#define MAX_PER_SECOND   10               // scritture (e notifiche) al secondo

static volatile bool     g_up = false, g_locked = false, g_secure = false, g_advWanted = false;
static volatile uint16_t g_handle = NO_CONN;           // collegamento in corso
static volatile bool     g_authOk = false;             // in questo collegamento l'abbinamento e riuscito
static volatile uint32_t g_connAt = 0, g_lastAct = 0;
static volatile uint8_t  g_failed = 0;
static BleDrvConfig      g_cfg;                         // copia: chi chiama puo buttare la sua
static NimBLEServer*     g_server = nullptr;
static NimBLECharacteristic* g_chr[BLE_MAX_CHARS];

struct Rate { uint32_t t0; uint8_t n; };
static Rate g_wr, g_nt;
static bool rateOk(Rate& r) {
  uint32_t now = (uint32_t)drvMillis();
  if (now - r.t0 >= 1000UL) { r.t0 = now; r.n = 0; }
  if (r.n >= MAX_PER_SECOND) return false;
  r.n++;
  return true;
}

// ---------- UUID ----------
static bool isHex(char c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'); }

// Controlla e scrive in minuscolo. 4 cifre (16 bit) o 36 caratteri canonici; non accetta altre lunghezze (la libreria le leggerebbe in altro modo).
static bool uuidNormalize(char* u) {
  size_t n = strnlen(u, BLE_UUID_TEXT);
  if (n == 4) {
    for (int i = 0; i < 4; i++) if (!isHex(u[i])) return false;
  } else if (n == 36) {
    for (int i = 0; i < 36; i++) {
      bool dash = (i == 8 || i == 13 || i == 18 || i == 23);
      if (dash ? u[i] != '-' : !isHex(u[i])) return false;
    }
  } else return false;
  for (size_t i = 0; i < n; i++) if (u[i] >= 'A' && u[i] <= 'F') u[i] = (char)(u[i] - 'A' + 'a');
  if (n == 4) {                                           // UUID a 16 bit riservati: servizi del sistema e descrittori
    unsigned v = (unsigned)strtoul(u, nullptr, 16);
    if (v == 0x1800 || v == 0x1801 || (v >= 0x2800 && v <= 0x29FF)) return false;
  }
  return true;
}

BleDrvErr drvBleCheckService(BleServiceDef& s) {
  s.uuid[BLE_UUID_TEXT - 1] = 0;
  if (s.n == 0) return BLE_OK;
  if (s.n > BLE_MAX_CHARS) return BLE_ERR_TOOMANY;
  if (!uuidNormalize(s.uuid)) return BLE_ERR_UUID;
  for (uint8_t i = 0; i < s.n; i++) {
    BleCharDef& c = s.ch[i];
    c.uuid[BLE_UUID_TEXT - 1] = 0;
    if (!uuidNormalize(c.uuid)) return BLE_ERR_UUID;
    if (c.props == 0 || (c.props & ~(BLE_PROP_READ | BLE_PROP_WRITE | BLE_PROP_NOTIFY))) return BLE_ERR_PROPS;
    if (strcmp(c.uuid, s.uuid) == 0) return BLE_ERR_DUP;
    for (uint8_t j = 0; j < i; j++) if (strcmp(c.uuid, s.ch[j].uuid) == 0) return BLE_ERR_DUP;
  }
  return BLE_OK;
}

// ---------- callback della libreria (task Bluetooth) ----------
static int idxOf(NimBLECharacteristic* c) {
  for (int i = 0; i < g_cfg.svc.n; i++) if (g_chr[i] == c) return i;
  return -1;
}
static void touch() { g_lastAct = (uint32_t)drvMillis(); }
static void dropLink() { if (g_server && g_handle != NO_CONN) g_server->disconnect((uint16_t)g_handle); }

class ChrCb : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic* c, NimBLEConnInfo&) override {
    int i = idxOf(c);
    if (i < 0) return;
    if (!g_secure) { dropLink(); return; }                // la libreria gia lo impedisce: seconda barriera
    if (!rateOk(g_wr)) return;                            // troppo veloce: scartata
    touch();
    NimBLEAttValue v = c->getValue();
    uint16_t n = v.length();
    if (n > BLE_VALUE_MAX || !g_cfg.ev.onWrite) return;
    g_cfg.ev.onWrite((uint8_t)i, v.data(), n);
  }
  void onRead(NimBLECharacteristic* c, NimBLEConnInfo&) override {
    int i = idxOf(c);
    if (i < 0) return;
    if (!g_secure) { dropLink(); return; }
    touch();
    if (g_cfg.ev.onRead) g_cfg.ev.onRead((uint8_t)i);
  }
  void onSubscribe(NimBLECharacteristic*, NimBLEConnInfo&, uint16_t) override {
    if (!g_secure) { dropLink(); return; }                // ci si iscrive solo dopo l'abbinamento
    touch();
  }
};

class LinkCb : public NimBLEServerCallbacks {
  void onConnect(NimBLEServer* s, NimBLEConnInfo& ci) override {
    if (g_handle != NO_CONN || g_locked) { s->disconnect(ci); return; }     // una sola connessione
    g_handle = ci.getConnHandle(); g_secure = false; g_authOk = false;
    uint32_t now = (uint32_t)drvMillis();
    g_connAt = now; g_lastAct = now;
    NimBLEDevice::startSecurity(ci.getConnHandle());      // chiede subito l'abbinamento
    if (g_cfg.ev.onLink) g_cfg.ev.onLink(true);
  }
  void onAuthenticationComplete(NimBLEConnInfo& ci) override {
    if (ci.getConnHandle() != g_handle) return;
    if (ci.isEncrypted() && ci.isAuthenticated()) { g_secure = true; g_authOk = true; g_failed = 0; touch(); }
    else if (g_server) g_server->disconnect(ci);          // cifrato ma non abbinato col codice: non basta
  }
  void onDisconnect(NimBLEServer*, NimBLEConnInfo& ci, int) override {
    if (ci.getConnHandle() != g_handle) return;           // era una connessione rifiutata
    bool paired = g_authOk;
    g_handle = NO_CONN; g_secure = false; g_authOk = false;
    if (g_cfg.ev.onLink) g_cfg.ev.onLink(false);
    if (!g_up) return;
    uint8_t failed = g_failed;                            // contatore toccato solo dalla task BLE: lettura, calcolo, scrittura
    if (!paired) { failed++; g_failed = failed; }
    if (!paired && failed >= MAX_FAILED) {            // troppi tentativi: niente piu annunci
      g_locked = true; g_advWanted = false;
      if (g_cfg.ev.onLocked) g_cfg.ev.onLocked();
      return;
    }
    if (g_advWanted) NimBLEDevice::startAdvertising();
  }
};

// ---------- accensione e spegnimento ----------
bool drvBlePrime() {
  if (g_up) return false;
  NimBLEDevice::init("");
  NimBLEDevice::createServer();
  NimBLEDevice::deinit(true);
  return true;
}

static void clearState() {
  g_up = false; g_locked = false; g_secure = false; g_advWanted = false; g_authOk = false;
  g_handle = NO_CONN; g_failed = 0; g_server = nullptr;
  memset(g_chr, 0, sizeof g_chr); memset(&g_wr, 0, sizeof g_wr); memset(&g_nt, 0, sizeof g_nt);
}

static uint32_t flagsOf(uint8_t p) {
  uint32_t f = 0;
  if (p & BLE_PROP_READ) f |= NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::READ_ENC | NIMBLE_PROPERTY::READ_AUTHEN;
  if (p & BLE_PROP_WRITE) f |= NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_ENC | NIMBLE_PROPERTY::WRITE_AUTHEN;
  if (p & BLE_PROP_NOTIFY) f |= NIMBLE_PROPERTY::NOTIFY;
  return f;
}

static bool advSetup() {
  NimBLEAdvertising* a = NimBLEDevice::getAdvertising();
  if (!a) return false;
  uint16_t itvl = (uint16_t)((uint32_t)g_cfg.advIntervalMs * 8 / 5);       // unita da 0,625 ms
  a->setMinInterval(itvl);
  a->setMaxInterval(itvl);
  a->enableScanResponse(true);                                              // il nome va nella risposta, l'UUID nell'annuncio
  a->setName(g_cfg.name);
  if (g_cfg.svc.n > 0) a->addServiceUUID(g_cfg.svc.uuid);
  return true;
}

static BleDrvErr beginImpl(const BleDrvConfig& c) {
  if (g_up) return BLE_ERR_STATE;
  g_cfg = c;
  g_cfg.name[BLE_NAME_MAX] = 0;
  size_t nl = strlen(g_cfg.name);
  if (nl == 0 || g_cfg.passkey < 100000 || g_cfg.passkey > 999999) return BLE_ERR_ARG;
  if (g_cfg.advIntervalMs < 100 || g_cfg.advIntervalMs > 2000) return BLE_ERR_ARG;
  BleDrvErr e = drvBleCheckService(g_cfg.svc);
  if (e != BLE_OK) return e;
  clearState();
  if (!NimBLEDevice::init(g_cfg.name)) return BLE_ERR_RAM;
  NimBLEDevice::setSecurityPasskey(g_cfg.passkey);
  NimBLEDevice::setSecurityIOCap(BLE_HS_IO_DISPLAY_ONLY);     // la scheda MOSTRA il codice, il telefono lo scrive
  NimBLEDevice::setSecurityAuth(false, true, true);           // niente memoria del telefono, protezione MITM, connessione sicura
  NimBLEServer* s = NimBLEDevice::createServer();
  if (!s) { NimBLEDevice::deinit(true); return BLE_ERR_RAM; }
  static LinkCb linkCb;                                        // oggetti fissi: con "new" ogni accensione ne lasciava due in RAM
  static ChrCb chrCb;
  s->setCallbacks(&linkCb, false);                             // false = la libreria non li cancella (sono statici)
  s->advertiseOnDisconnect(false);                             // l'annuncio riparte solo se richiesto (onDisconnect)
  if (g_cfg.svc.n > 0) {
    NimBLEService* svc = s->createService(g_cfg.svc.uuid);
    if (!svc) { NimBLEDevice::deinit(true); return BLE_ERR_RAM; }
    for (uint8_t i = 0; i < g_cfg.svc.n; i++) {
      g_chr[i] = svc->createCharacteristic(g_cfg.svc.ch[i].uuid, flagsOf(g_cfg.svc.ch[i].props), BLE_VALUE_MAX);
      if (!g_chr[i]) { clearState(); NimBLEDevice::deinit(true); return BLE_ERR_RAM; }
      g_chr[i]->setCallbacks(&chrCb);
    }
  }
  if (!s->start() || !advSetup()) { clearState(); NimBLEDevice::deinit(true); return BLE_ERR_RAM; }
  g_server = s;
  g_up = true;
  g_advWanted = c.advertise;
  if (c.advertise) NimBLEDevice::startAdvertising();
  return BLE_OK;
}

// Se l'accensione fallisce il codice non resta in memoria.
BleDrvErr drvBleBegin(const BleDrvConfig& c) {
  BleDrvErr r = beginImpl(c);
  if (r != BLE_OK && r != BLE_ERR_STATE) { g_cfg.passkey = 0; NimBLEDevice::setSecurityPasskey(123456); }
  return r;
}

void drvBleEnd() {
  if (!g_up) return;
  g_up = false;                                                // prima: i callback in corso non riavviano l'annuncio
  clearState();
  g_cfg.passkey = 0;                                           // il codice non resta in memoria
  NimBLEDevice::deinit(true);                                  // true = libera anche server e servizi (riparte da zero alla prossima accensione)
  NimBLEDevice::setSecurityPasskey(123456);                    // la libreria non tiene il nostro codice (123456 e il suo valore di partenza)
}

bool drvBleIsUp() { return g_up; }
bool drvBleConnected() { return g_up && g_handle != NO_CONN; }
bool drvBleSecure() { return g_up && g_handle != NO_CONN && g_secure; }
bool drvBleLocked() { return g_locked; }

bool drvBleAdvertise(bool on) {
  if (!g_up || g_locked) return false;
  g_advWanted = on;
  return on ? NimBLEDevice::startAdvertising() : NimBLEDevice::stopAdvertising();
}

BleDrvErr drvBleSetValue(uint8_t idx, const uint8_t* data, uint16_t len) {
  if (!g_up) return BLE_ERR_STATE;
  if (idx >= g_cfg.svc.n || !g_chr[idx] || (!data && len)) return BLE_ERR_ARG;
  if (len > BLE_VALUE_MAX) return BLE_ERR_SIZE;
  g_chr[idx]->setValue(data, len);
  return BLE_OK;
}

BleDrvErr drvBleNotify(uint8_t idx, const uint8_t* data, uint16_t len) {
  if (!g_up || g_handle == NO_CONN) return BLE_ERR_STATE;
  if (idx >= g_cfg.svc.n || !g_chr[idx] || (!data && len)) return BLE_ERR_ARG;
  if (!(g_cfg.svc.ch[idx].props & BLE_PROP_NOTIFY)) return BLE_ERR_PROPS;
  if (len > BLE_VALUE_MAX) return BLE_ERR_SIZE;
  if (!g_secure) return BLE_ERR_SECURE;                        // niente notifiche a un collegamento non abbinato
  uint16_t mtu = g_server ? g_server->getPeerMTU((uint16_t)g_handle) : 0;
  if (mtu < 4 || len > mtu - 3) return BLE_ERR_SIZE;           // un valore piu lungo del pacchetto verrebbe tagliato
  if (!rateOk(g_nt)) return BLE_ERR_RATE;
  g_chr[idx]->setValue(data, len);
  return g_chr[idx]->notify(data, len, (uint16_t)g_handle) ? BLE_OK : BLE_ERR_STATE;   // false: il telefono non si e iscritto
}

void drvBleDisconnect() { if (g_up) dropLink(); }

void drvBleTick() {
  if (!g_up || g_handle == NO_CONN) return;
  uint32_t now = (uint32_t)drvMillis();
  if (!g_secure) { if (now - g_connAt > PAIR_WAIT_MS) dropLink(); }
  else if (now - g_lastAct > IDLE_MS) dropLink();
}
#endif
