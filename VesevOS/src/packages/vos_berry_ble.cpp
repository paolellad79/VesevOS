// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_berry_ble.cpp: funzioni Berry del Bluetooth. Politica e ciclo di vita: vos_ble.h.
//
// DUE INSIEMI DI FUNZIONI
//   - App (.vab) con use=ble nel manifest: ble_on, ble_off, ble_status, ble_name, ble_adv, ble_service, ble_set, ble_notify,
//     ble_disconnect. Le funzioni on_write(uuid, dati) e on_read(uuid) dell'app sono chiamate dal ciclo dell'app (berryBleDispatch),
//     MAI dal task Bluetooth: se la funzione sbaglia, l'app si ferma e il Bluetooth si spegne.
//   - Script (.vsb) e "berry <codice>": solo ble_status; se sono stati lanciati dall'Admin (script run, pagina Script, berry) anche
//     ble_off (spegne in ogni caso) e ble_passkey (il codice di abbinamento). Uno script avviato da una regola, da MQTT o dalla rete
//     tra schede non puo vedere il codice: potrebbe mandarlo fuori.
// Accendere e spegnere NON avviene qui: si mette una richiesta e la esegue il ciclo principale (bleTick). Nessun oggetto con
// distruttore resta vivo quando si chiama be_raise (longjmp).
#include "vos_berry_int.h"
#include "../core/vos_common.h"
#if VOS_WITH_BERRY
#include "../third_party/berry/src/berry.h"
#include "../core/vos_i18n.h"
#include "../core/vos_log.h"
#include "../core/vos_errbuf.h"
#include "vos_ble.h"

#if VOS_WITH_BLE
// ---------- aiuti ----------
static const char* argStr(bvm* vm, int i, const char* fn) {              // argomento testo, o errore Berry
  if (be_top(vm) < i || !be_isstring(vm, i)) { char m[48]; snprintf(m, sizeof m, "%s: argomento %d: serve un testo", fn, i); be_raise(vm, "type_error", m); }
  return be_tostring(vm, i);
}
static void fail(bvm* vm, const ErrBuf& e) { be_raise(vm, "ble_error", e.c_str()); }

// dati da inviare: testo (anche con byte qualunque) oppure un numero 0-255 (un byte). Torna la lunghezza, o -1 se il tipo non va.
static int argData(bvm* vm, int i, uint8_t* one, const uint8_t** p) {
  if (be_top(vm) >= i && be_isstring(vm, i)) { *p = (const uint8_t*)be_tostring(vm, i); return be_strlen(vm, i); }
  if (be_top(vm) >= i && be_isint(vm, i)) {
    bint v = be_toint(vm, i);
    if (v < 0 || v > 255) return -1;
    *one = (uint8_t)v; *p = one; return 1;
  }
  return -1;
}

// ---------- ble_status, ble_off, ble_passkey ----------
static int bfBleStatus(bvm* vm) {
  char st[BLE_STATUS_MAX]; bleStatusLine(st, sizeof st);
  be_pushstring(vm, st);                     // "off", "on ...", "connected ..." (la prima parola e lo stato)
  be_return(vm);
}
static int bfBleOffApp(bvm* vm) {
  ErrBuf e;
  if (!bleAppOff(g_name, e)) fail(vm, e);
  be_return_nil(vm);
}
static int bfBleOffAdmin(bvm* vm) { bleStop(); be_return_nil(vm); }      // lanciato dall'Admin: spegne in ogni caso
static int bfBlePasskey(bvm* vm) {
  char pin[8];
  if (!blePasskey(pin, sizeof pin)) { be_return_nil(vm); }              // spento: nil
  be_pushstring(vm, pin);
  be_return(vm);
}

// ---------- funzioni delle app ----------
static int bfBleOn(bvm* vm) {
  long min = 0;
  if (be_top(vm) >= 1) {
    if (!be_isint(vm, 1)) be_raise(vm, "type_error", "ble_on(minuti): serve un numero intero");
    min = (long)be_toint(vm, 1);
    if (min < 0 || min > BLE_MINUTES_MAX) be_raise(vm, "value_error", "ble_on(minuti): da 0 a 60 (0 o niente = finche l'app gira)");
  }
  ErrBuf e;
  if (!bleAppOn(g_name, (uint32_t)min, e)) fail(vm, e);
  be_return_nil(vm);
}
static int bfBleName(bvm* vm) {
  const char* n = argStr(vm, 1, "ble_name");
  ErrBuf e;
  if (!bleAppName(g_name, n, e)) fail(vm, e);
  be_return_nil(vm);
}
static int bfBleAdv(bvm* vm) {
  bool on;
  if (be_top(vm) >= 1 && be_isbool(vm, 1)) on = be_tobool(vm, 1);
  else if (be_top(vm) >= 1 && be_isint(vm, 1)) on = be_toint(vm, 1) != 0;
  else { be_raise(vm, "type_error", "ble_adv(vero/falso)"); return 0; }
  ErrBuf e;
  if (!bleAppAdv(g_name, on, e)) fail(vm, e);
  be_return_nil(vm);
}

// ble_service("uuid-servizio", "uuid-1", "rn", "uuid-2", "w", ...): fino a 4 coppie (uuid, proprieta); proprieta = lettere r, w, n
static int bfBleService(bvm* vm) {
  int argc = be_top(vm);
  if (argc < 3 || (argc % 2) != 1 || argc > 1 + 2 * BLE_MAX_CHARS) be_raise(vm, "type_error", "ble_service(\"uuid\", \"uuid1\", \"rwn\", ...): servizio e da 1 a 4 coppie (uuid, proprieta)");
  BleServiceDef d;
  memset(&d, 0, sizeof d);
  const char* su = argStr(vm, 1, "ble_service");
  if (strlen(su) >= BLE_UUID_TEXT) be_raise(vm, "value_error", "ble_service: UUID troppo lungo");
  strcpy(d.uuid, su);
  d.n = (uint8_t)((argc - 1) / 2);
  for (int k = 0; k < d.n; k++) {
    const char* cu = argStr(vm, 2 + 2 * k, "ble_service");
    const char* pr = argStr(vm, 3 + 2 * k, "ble_service");
    if (strlen(cu) >= BLE_UUID_TEXT) be_raise(vm, "value_error", "ble_service: UUID troppo lungo");
    strcpy(d.ch[k].uuid, cu);
    uint8_t f = 0;
    for (const char* q = pr; *q; q++) {
      uint8_t b = *q == 'r' ? BLE_PROP_READ : *q == 'w' ? BLE_PROP_WRITE : *q == 'n' ? BLE_PROP_NOTIFY : 0;
      if (!b || (f & b)) be_raise(vm, "value_error", "ble_service: proprieta = lettere r (read), w (write), n (notify), senza ripetizioni");
      f |= b;
    }
    d.ch[k].props = f;
  }
  ErrBuf e;
  if (!bleAppService(g_name, d, e)) fail(vm, e);
  be_return_nil(vm);
}

static int bfBleSet(bvm* vm) {
  const char* u = argStr(vm, 1, "ble_set");
  uint8_t one; const uint8_t* p = nullptr;
  int n = argData(vm, 2, &one, &p);
  if (n < 0) be_raise(vm, "type_error", "ble_set(uuid, dati): dati = testo o numero 0-255");
  ErrBuf e;
  if (!bleAppSet(g_name, u, p, (size_t)n, e)) fail(vm, e);
  be_return_nil(vm);
}
static int bfBleNotify(bvm* vm) {
  const char* u = argStr(vm, 1, "ble_notify");
  uint8_t one; const uint8_t* p = nullptr;
  int n = argData(vm, 2, &one, &p);
  if (n < 0) be_raise(vm, "type_error", "ble_notify(uuid, dati): dati = testo o numero 0-255");
  ErrBuf e;
  if (!bleAppNotify(g_name, u, p, (size_t)n, e)) fail(vm, e);
  be_return_nil(vm);
}
static int bfBleDisconnect(bvm* vm) {
  ErrBuf e;
  if (!bleAppDisconnect(g_name, e)) fail(vm, e);
  be_return_nil(vm);
}

void berryBleRegister(bvm* vm, bool app, bool admin) {
  be_regfunc(vm, "ble_status", bfBleStatus);
  if (!app) {                                                     // script: solo gestione, e solo se lanciati dall'Admin
    if (admin) { be_regfunc(vm, "ble_off", bfBleOffAdmin); be_regfunc(vm, "ble_passkey", bfBlePasskey); }
    return;
  }
  be_regfunc(vm, "ble_on", bfBleOn);          be_regfunc(vm, "ble_off", bfBleOffApp);
  be_regfunc(vm, "ble_name", bfBleName);      be_regfunc(vm, "ble_adv", bfBleAdv);
  be_regfunc(vm, "ble_service", bfBleService); be_regfunc(vm, "ble_set", bfBleSet);
  be_regfunc(vm, "ble_notify", bfBleNotify);  be_regfunc(vm, "ble_disconnect", bfBleDisconnect);
}

// ---------- eventi dal telefono: chiamati dal ciclo dell'app (stesso task di Berry, con i limiti di tempo di sempre) ----------
// on_write(uuid, dati) per ogni scrittura; on_read(uuid) dopo ogni lettura: se restituisce un testo o un numero 0-255 diventa
// il valore della lettura SUCCESSIVA (la lettura in corso e gia stata servita col valore di prima: la radio non aspetta Berry).
// Torna 0 = bene, altro = errore Berry (l'eccezione resta nella pila: la gestisce il chiamante). did = c'era un evento.
int berryBleDispatch(bvm* vm, bool& did) {
  did = false;
  BleAppEvent ev;
  if (!bleAppPopEvent(g_name, ev)) return 0;
  did = true;
  char uuid[BLE_UUID_TEXT];
  if (!bleAppUuid(g_name, ev.idx, uuid, sizeof uuid)) return 0;
  bool wr = (ev.kind == BLE_EV_WRITE);
  int base = be_top(vm);
  if (!be_getglobal(vm, wr ? "on_write" : "on_read") || !be_isfunction(vm, -1)) { be_pop(vm, be_top(vm) - base); return 0; }   // l'app non l'ha definita
  be_pushstring(vm, uuid);
  int argc = 1;
  if (wr) { be_pushnstring(vm, (const char*)ev.data, ev.len); argc = 2; }
  int r = be_pcall(vm, argc);
  if (r != 0) return r;
  if (!wr) {                                                      // il risultato sta dove era la funzione
    int at = -(argc + 1);
    uint8_t one; const uint8_t* p = nullptr; int n = -1;
    if (be_isstring(vm, at)) { p = (const uint8_t*)be_tostring(vm, at); n = be_strlen(vm, at); }
    else if (be_isint(vm, at) && be_toint(vm, at) >= 0 && be_toint(vm, at) <= 255) { one = (uint8_t)be_toint(vm, at); p = &one; n = 1; }
    if (n >= 0) {
      ErrBuf e;
      if (!bleAppSet(g_name, uuid, p, (size_t)n, e)) vlog("BLE: valore di on_read non usato: %s", e.c_str());
    }
  }
  be_pop(vm, be_top(vm) - base);
  return 0;
}

// ---------- fine del programma / rete di sicurezza ----------
static bool ownerAlive(const char* owner) {
  char nm[40];
  return berryAppRunning(nm, sizeof nm) && strcmp(nm, owner) == 0;
}
void berryBleEnded(const char* appName) { bleAppEnded(appName); }
void berryBleInit() { bleSetOwnerCheck(ownerAlive); }
#else
void berryBleRegister(bvm*, bool, bool) {}
int  berryBleDispatch(bvm*, bool& did) { did = false; return 0; }
void berryBleEnded(const char*) {}
void berryBleInit() {}
#endif
#endif
