// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_drv_ble.cpp (strato 1: driver Bluetooth, vedi vos_drv_ble.h)
#include "vos_drv_ble.h"

#if VOS_WITH_BLE
#include <NimBLEDevice.h>                 // libreria NimBLE-Arduino 2.x: usa molta meno RAM del Bluetooth di Arduino (misurato: quello costava 74 KB)

#define BLE_RX_MAX 200                    // un testo piu lungo viene scartato

static volatile bool g_up = false, g_link = false;
static NimBLECharacteristic* g_tx = nullptr;
static BleRxCb g_onRx = nullptr;
static BleLinkCb g_onLink = nullptr;

class RxCb : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic* c, NimBLEConnInfo&) override {
    NimBLEAttValue v = c->getValue();
    if (v.length() > BLE_RX_MAX || !g_onRx) return;
    String t; t.reserve(v.length());
    const uint8_t* d = v.data();
    for (size_t i = 0; i < v.length(); i++) t += (char)d[i];
    g_onRx(t);
  }
};
class LinkCb : public NimBLEServerCallbacks {
  void onConnect(NimBLEServer*, NimBLEConnInfo&) override { g_link = true; if (g_onLink) g_onLink(true); }
  void onDisconnect(NimBLEServer*, NimBLEConnInfo&, int) override {
    g_link = false; if (g_onLink) g_onLink(false);
    if (g_up) NimBLEDevice::startAdvertising();                 // spento apposta: non riprendere
  }
};

bool drvBlePrime() {
  if (g_up) return false;
  NimBLEDevice::init("");
  NimBLEDevice::createServer();
  NimBLEDevice::deinit(true);
  return true;
}

bool drvBleBegin(const BleDrvConfig& c) {
  if (g_up) return true;
  g_onRx = c.onRx; g_onLink = c.onLink;
  NimBLEDevice::init(c.devName);
  NimBLEDevice::setSecurityPasskey(c.passkey);
  NimBLEDevice::setSecurityIOCap(BLE_HS_IO_DISPLAY_ONLY);     // la scheda MOSTRA il codice, il telefono lo scrive
  NimBLEDevice::setSecurityAuth(false, true, true);           // niente memoria del telefono, protezione MITM, connessione sicura
  NimBLEServer* s = NimBLEDevice::createServer();
  static LinkCb linkCb;                                        // oggetti fissi: con "new" ogni accensione ne lasciava due in RAM
  s->setCallbacks(&linkCb, false);                             // false = la libreria non li cancella (sono statici)
  NimBLEService* svc = s->createService(c.svcUuid);
  NimBLECharacteristic* rx = svc->createCharacteristic(c.rxUuid, NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_ENC | NIMBLE_PROPERTY::WRITE_AUTHEN);
  static RxCb rxCb;
  rx->setCallbacks(&rxCb);
  g_tx = svc->createCharacteristic(c.txUuid, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::READ_ENC | NIMBLE_PROPERTY::READ_AUTHEN | NIMBLE_PROPERTY::NOTIFY);
  if (c.hello) g_tx->setValue((const uint8_t*)c.hello, (uint16_t)strlen(c.hello));
  s->start();
  NimBLEDevice::getAdvertising()->addServiceUUID(c.svcUuid);
  NimBLEDevice::startAdvertising();
  g_up = true;
  return true;
}

void drvBleEnd() {
  if (!g_up) return;
  g_up = false; g_tx = nullptr; g_link = false;
  NimBLEDevice::deinit(true);                                  // true = libera anche server e servizi (riparte da zero alla prossima accensione)
}

bool drvBleIsUp() { return g_up; }
bool drvBleConnected() { return g_link; }

bool drvBleSend(const String& text) {
  if (!g_up || !g_tx) return false;
  g_tx->setValue((const uint8_t*)text.c_str(), (uint16_t)text.length());
  g_tx->notify();
  return true;
}

void drvBleAdvertise(bool on) {
  if (!g_up) return;
  if (on) NimBLEDevice::startAdvertising(); else NimBLEDevice::stopAdvertising();
}
#endif
