// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_func.cpp (vedi vos_func.h)
#include "vos_func.h"
#include <stdio.h>
#include <string.h>
#include "../core/vos_config.h"
#include "../core/vos_i18n.h"
#include "../core/vos_log.h"

#ifndef TRK
#define TRK(s) s      // testo che si traduce piu tardi, quando serve (lo scanner delle traduzioni lo trova lo stesso)
#endif

namespace {

struct Info { const char* desc; uint8_t rr, rw; };

// Stesso ordine di kFuncId (core/vos_func_ids.h).
const Info kInfo[kFuncN] = {
  { TRK("Scrivere nel registro del sistema."),                              ROLE_GUEST, ROLE_GUEST },   // log
  { TRK("Leggere e scrivere file sulla microSD."),                          ROLE_OPER,  ROLE_ADMIN },   // sd
  { TRK("Leggere e comandare le periferiche gia configurate."),             ROLE_OPER,  ROLE_ADMIN },   // dev
  { TRK("Leggere e comandare i pin dati a un driver."),                     ROLE_OPER,  ROLE_ADMIN },   // gpio
  { TRK("Parlare con i sensori sul bus I2C dato a un driver."),             ROLE_OPER,  ROLE_ADMIN },   // i2c
  { TRK("Parlare con i moduli sul bus SPI dato a un driver."),              ROLE_ADMIN, ROLE_ADMIN },   // spi
  { TRK("Leggere l'ora e il tempo di accensione, fare pause brevi."),       ROLE_GUEST, ROLE_GUEST },   // time
  { TRK("Vedere file e cartelle (nomi, dimensioni, date) e usare la cartella dei dati dell'app."),               ROLE_OPER,  ROLE_OPER },    // files
  { TRK("Usare il Bluetooth."),                                             ROLE_ADMIN, ROLE_ADMIN },   // ble
};

// Il gruppo e una delle parole di use= (separate da spazi o virgole)?
bool inUse(const char* use, const char* gruppo) {
  size_t gl = strlen(gruppo);
  const char* q = use ? use : "";
  while (*q) {
    while (*q == ' ' || *q == ',') q++;
    const char* s = q;
    while (*q && *q != ' ' && *q != ',') q++;
    if ((size_t)(q - s) == gl && !strncmp(s, gruppo, gl)) return true;
  }
  return false;
}

}  // namespace

bool funcOnIdx(int i) { return i < 0 || i >= kFuncN || !(cfg.funcOff & (1u << i)); }
bool funcOn(const char* id) { return funcOnIdx(funcIndex(id)); }
const char* funcDesc(int i) { return i >= 0 && i < kFuncN ? tr(kInfo[i].desc) : ""; }
int funcRoleRead(int i) { return i >= 0 && i < kFuncN ? kInfo[i].rr : ROLE_ADMIN; }
int funcRoleWrite(int i) { return i >= 0 && i < kFuncN ? kInfo[i].rw : ROLE_ADMIN; }

static FuncUsersFn g_users = nullptr;
static FuncStopFn  g_stop = nullptr;
void funcHooksSet(FuncUsersFn users, FuncStopFn stop) { g_users = users; g_stop = stop; }

int funcUsers(int i) { return i >= 0 && i < kFuncN && g_users ? g_users(kFuncId[i]) : 0; }

bool funcSet(int i, bool on, ErrBuf& err) {
  if (i < 0 || i >= kFuncN) { err = tr("Funzionalita sconosciuta"); return false; }
  if (funcOnIdx(i) == on) return true;
  if (on) cfg.funcOff &= (uint16_t)~(1u << i); else cfg.funcOff |= (uint16_t)(1u << i);
  cfgSetOrigin("funzionalita");
  cfgSave();
  vlog("FUNZIONALITA: %s %s", kFuncId[i], on ? "concessa" : "tolta");
  if (!on && g_stop) g_stop(kFuncId[i]);                        // chi la usa adesso viene fermato: alla prossima chiamata non avrebbe piu il permesso
  return true;
}

const char* funcMissing(const char* use) {
  for (int i = 0; i < kFuncN; i++) if (!funcOnIdx(i) && inUse(use, kFuncId[i])) return kFuncId[i];
  return nullptr;
}

bool funcJson(HeapBuf& out) {
  out.add("{\"list\":[");
  for (int i = 0; i < kFuncN; i++) {
    if (i) out.add(',');
    out.add("{\"id\":\""); out.addJson(kFuncId[i]);
    out.add("\",\"d\":\""); out.addJson(funcDesc(i));
    out.addf("\",\"rr\":%d,\"rw\":%d,\"on\":%s,\"u\":%d}", (int)kInfo[i].rr, (int)kInfo[i].rw, funcOnIdx(i) ? "true" : "false", funcUsers(i));
  }
  out.add("]}");
  return !(out.oom() || out.over());
}
