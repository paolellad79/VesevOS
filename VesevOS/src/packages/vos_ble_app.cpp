// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_ble_app.cpp
// STRATO 6 (packages): le operazioni che un programma Berry con il permesso ble puo fare sul Bluetooth (le chiama vos_berry_ble.cpp)
// e la coda degli eventi dal telefono. Il programma e riconosciuto dal nome (owner): solo il proprietario puo toccare il Bluetooth.
#include "vos_ble_int.h"
#include "../core/vos_i18n.h"
#include <strings.h>
#include <freertos/FreeRTOS.h>                         // portMUX_TYPE

#if VOS_WITH_BLE
using namespace vosble;

static const char* drvErrText(BleDrvErr e) {
  switch (e) {
    case BLE_ERR_UUID:    return tr("UUID non valido (4 cifre esadecimali o 36 caratteri 8-4-4-4-12; alcuni sono riservati)");
    case BLE_ERR_PROPS:   return tr("Proprieta non valide (read, write, notify)");
    case BLE_ERR_TOOMANY: return tr("Troppe caratteristiche (massimo 4)");
    case BLE_ERR_DUP:     return tr("UUID ripetuto");
    case BLE_ERR_RAM:     return tr("Memoria insufficiente per il Bluetooth: chiudi la pagina web e riprova");
    case BLE_ERR_SECURE:  return tr("Il telefono non e ancora abbinato");
    case BLE_ERR_RATE:    return tr("Troppi messaggi al secondo (massimo 10)");
    case BLE_ERR_SIZE:    return tr("Valore troppo lungo (massimo 128 byte, e non oltre il pacchetto del telefono)");
    case BLE_ERR_STATE:   return tr("Nessun telefono collegato o iscritto");
    default:              return tr("Parametro non valido");
  }
}

// proprietario: errore se il Bluetooth e di un altro programma o se il programma non l'ha ancora preso
static bool mine(const char* owner, ErrBuf& err) {
  if (!owner || !owner[0] || !g_owner[0]) { err = tr("Bluetooth spento: chiama prima ble_on()"); return false; }
  if (strcmp(owner, g_owner) != 0) { err.setf(tr("Bluetooth gia in uso dal programma %s"), g_owner); return false; }
  return true;
}

static bool nameOk(const char* s) {
  size_t n = s ? strlen(s) : 0;
  if (n < 1 || n > BLE_NAME_MAX || s[0] == ' ' || s[n - 1] == ' ') return false;
  for (size_t i = 0; i < n; i++) {
    char c = s[i];
    if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == ' ' || c == '_' || c == '.' || c == '-')) return false;
  }
  return true;
}

bool bleAppName(const char* owner, const char* name, ErrBuf& err) {
  VosLock lk(g_mx);
  if (!nameOk(name)) { err = tr("Nome non valido: 1-20 caratteri (lettere, cifre, spazio, punto, trattino); non mettere dati personali"); return false; }
  if (!claim(owner, err)) return false;
  if (g_phase != IDLE) { err = tr("Imposta il nome prima di ble_on()"); return false; }
  snprintf(g_name, sizeof g_name, "%s", name);
  return true;
}

bool bleAppAdv(const char* owner, bool on, ErrBuf& err) {
  VosLock lk(g_mx);
  if (!claim(owner, err)) return false;
  g_adv = on;
  if (g_phase == UP && !drvBleAdvertise(on) && on) { err = tr("Annuncio non possibile (Bluetooth bloccato o radio ferma)"); return false; }
  return true;
}

bool bleAppService(const char* owner, BleServiceDef& def, ErrBuf& err) {
  VosLock lk(g_mx);
  if (!claim(owner, err)) return false;
  if (g_phase != IDLE) { err = tr("Definisci il servizio prima di ble_on()"); return false; }
  BleDrvErr e = drvBleCheckService(def);
  if (e != BLE_OK) { err = drvErrText(e); return false; }
  g_def = def;
  return true;
}

static int findChar(const char* uuid) {
  if (!uuid) return -1;
  for (int i = 0; i < g_def.n; i++) if (strcasecmp(uuid, g_def.ch[i].uuid) == 0) return i;
  return -1;
}

bool bleAppSet(const char* owner, const char* uuid, const uint8_t* d, size_t n, ErrBuf& err) {
  VosLock lk(g_mx);
  if (!mine(owner, err)) return false;
  if (g_phase != UP) { err = tr("Bluetooth spento: chiama prima ble_on()"); return false; }
  int i = findChar(uuid);
  if (i < 0) { err = tr("Caratteristica sconosciuta (non e nel servizio)"); return false; }
  if (n > BLE_VALUE_MAX) { err = drvErrText(BLE_ERR_SIZE); return false; }
  BleDrvErr e = drvBleSetValue((uint8_t)i, d, (uint16_t)n);
  if (e != BLE_OK) { err = drvErrText(e); return false; }
  return true;
}

bool bleAppNotify(const char* owner, const char* uuid, const uint8_t* d, size_t n, ErrBuf& err) {
  VosLock lk(g_mx);
  if (!mine(owner, err)) return false;
  if (g_phase != UP) { err = tr("Bluetooth spento: chiama prima ble_on()"); return false; }
  int i = findChar(uuid);
  if (i < 0) { err = tr("Caratteristica sconosciuta (non e nel servizio)"); return false; }
  if (!(g_def.ch[i].props & BLE_PROP_NOTIFY)) { err = tr("La caratteristica non ha la proprieta notify"); return false; }
  if (n > BLE_VALUE_MAX) { err = drvErrText(BLE_ERR_SIZE); return false; }
  BleDrvErr e = drvBleNotify((uint8_t)i, d, (uint16_t)n);
  if (e != BLE_OK) { err = drvErrText(e); return false; }
  return true;
}

bool bleAppDisconnect(const char* owner, ErrBuf& err) {
  VosLock lk(g_mx);
  if (!mine(owner, err)) return false;
  if (g_phase == UP) drvBleDisconnect();
  return true;
}

bool bleAppUuid(const char* owner, uint8_t idx, char* out, size_t n) {
  VosLock lk(g_mx);
  if (!owner || !g_owner[0] || strcmp(owner, g_owner) != 0 || idx >= g_def.n || n < BLE_UUID_TEXT) return false;
  snprintf(out, n, "%s", g_def.ch[idx].uuid);
  return true;
}

// ---- coda degli eventi: la riempie il task Bluetooth, la svuota il programma (stesso blocco breve per tutti) ----
#define BLE_EVQ 6
static BleAppEvent g_q[BLE_EVQ];
static uint8_t g_qh = 0, g_qn = 0;
static portMUX_TYPE g_qmux = portMUX_INITIALIZER_UNLOCKED;

static void qPush(const BleAppEvent& e) {
  portENTER_CRITICAL(&g_qmux);
  bool dup = false;
  if (e.kind == BLE_EV_READ) for (uint8_t k = 0; k < g_qn; k++) { const BleAppEvent& x = g_q[(g_qh + k) % BLE_EVQ]; if (x.kind == BLE_EV_READ && x.idx == e.idx) dup = true; }
  if (!dup && g_qn < BLE_EVQ) { g_q[(g_qh + g_qn) % BLE_EVQ] = e; g_qn++; }        // coda piena: l'evento si perde
  portEXIT_CRITICAL(&g_qmux);
}
static void onWrite(uint8_t idx, const uint8_t* d, uint16_t len) {
  if (len > BLE_VALUE_MAX || (!d && len)) return;
  BleAppEvent e; e.kind = BLE_EV_WRITE; e.idx = idx; e.len = (uint8_t)len;
  if (len) memcpy(e.data, d, len);
  qPush(e);
}
static void onRead(uint8_t idx) { BleAppEvent e; e.kind = BLE_EV_READ; e.idx = idx; e.len = 0; qPush(e); }

bool bleAppPopEvent(const char* owner, BleAppEvent& ev) {
  {
    VosLock lk(g_mx);
    if (!owner || !g_owner[0] || strcmp(owner, g_owner) != 0 || g_phase != UP) return false;
  }
  bool ok = false;
  portENTER_CRITICAL(&g_qmux);
  if (g_qn > 0) { ev = g_q[g_qh]; g_qh = (uint8_t)((g_qh + 1) % BLE_EVQ); g_qn--; ok = true; }
  portEXIT_CRITICAL(&g_qmux);
  return ok;
}

void vosble::queueClear() { portENTER_CRITICAL(&g_qmux); g_qh = g_qn = 0; portEXIT_CRITICAL(&g_qmux); }

BleDrvEvents* vosble::driverEvents() {
  static BleDrvEvents ev = { onWrite, onRead, onLinkEvent, nullptr };
  return &ev;
}
#endif
