// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_drvcmd.cpp (vedi vos_drvcmd.h): registro dei comandi dei driver e loro esecuzione.
#include "vos_drvcmd.h"
#if VOS_WITH_DRVCMD && VOS_WITH_BERRY
#include <stdio.h>
#include <string.h>
#include "../core/vos_i18n.h"
#include "../core/vos_lock.h"
#include "../core/vos_log.h"
#include "../devices/vos_mod_cmd.h"
#include "vos_app.h"
#if VOS_WITH_ELF
#include "vos_vde.h"
#endif

namespace {

ModCmdEntry g_tab[DRVCMD_MAX];
int         g_n = 0;
bool        g_valid = false;

VosMutex& mx() { static VosMutex m = [] { VosMutex v; v.init(); return v; }(); return m; }

// Rilegge /devs se serve. Si chiama con il blocco preso.
void load() {
  if (g_valid) return;
  g_n = modCmdList(g_tab, DRVCMD_MAX);
#if VOS_WITH_ELF
  if (g_n < DRVCMD_MAX) g_n += vdeComandiLista(g_tab + g_n, DRVCMD_MAX - g_n);   // anche i driver in ELF (.vde)
#endif
  g_valid = true;
}

void copyOut(const ModCmdEntry& e, DrvCmd& d) {
  snprintf(d.driver, sizeof d.driver, "%s", e.driver);
  snprintf(d.cmd, sizeof d.cmd, "%s", e.c.cmd);
  snprintf(d.sub, sizeof d.sub, "%s", e.c.sub);
  d.admin = e.c.admin; d.rest = e.c.rest;
}

}  // namespace

void drvCmdRefresh() { VosLock L(mx()); g_valid = false; }

bool drvCmdHas(const char* cmd) {
  VosLock L(mx()); load();
  for (int i = 0; i < g_n; i++) if (!strcmp(g_tab[i].c.cmd, cmd)) return true;
  return false;
}

bool drvCmdFind(const char* cmd, const char* sub, DrvCmd& out) {
  VosLock L(mx()); load();
  for (int i = 0; i < g_n; i++) if (!strcmp(g_tab[i].c.cmd, cmd) && !strcmp(g_tab[i].c.sub, sub)) { copyOut(g_tab[i], out); return true; }
  return false;
}

int drvCmdSubs(const char* cmd, DrvCmd* out, int mx_) {
  VosLock L(mx()); load();
  int n = 0;
  for (int i = 0; i < g_n && n < mx_; i++) if (!strcmp(g_tab[i].c.cmd, cmd)) copyOut(g_tab[i], out[n++]);
  return n;
}

int drvCmdWords(char (*out)[13], int mx_) {
  VosLock L(mx()); load();
  int n = 0;
  for (int i = 0; i < g_n && n < mx_; i++) {
    bool dup = false;
    for (int k = 0; k < n; k++) if (!strcmp(out[k], g_tab[i].c.cmd)) dup = true;
    if (!dup) snprintf(out[n++], 13, "%s", g_tab[i].c.cmd);
  }
  return n;
}

// La pagina del driver e un'app che resta in esecuzione e tiene occupato l'unico interprete: il comando la mette in pausa (la ferma)
// e a comando finito la riavvia. "Occupato" resta solo per un altro script o per l'app di un altro driver (un comando alla volta).
// Costo: lo stato dell'app si azzera (per esempio una prova a 9 passi in corso si interrompe).
bool drvCmdRun(const DrvCmd& c, const CmdCall& call, bool callerAdmin, HeapBuf& out, ErrBuf& err) {
  if (!berryCmdArgsOk(call, err)) return false;
#if VOS_WITH_ELF
  if (vdeEsiste(c.driver)) {                                 // driver in ELF: lo stesso registro, un altro motore
    const char* parole[CMD_ARGS_MAX];
    for (int i = 0; i < call.n && i < CMD_ARGS_MAX; i++) parole[i] = call.arg[i];
    return vdeComando(c.driver, c.sub, callerAdmin && c.admin, call.n, parole, out, err);
  }
#endif
  HeapBuf code(MOD_CMDCODE_MAX + 128); char use[100]; ErrBuf e;
  if (!modCmdCode(c.driver, code, use, sizeof use, e)) { err = e.c_str(); return false; }
  code.add("\non_cmd(cmd(), args())\n");                      // il driver definisce on_cmd(sub, args); qui lo si chiama
  if (code.oom() || code.over()) { err = tr("Memoria insufficiente"); return false; }
  bool admin = callerAdmin && c.admin;                       // nessun diritto in piu: scrivere sulla microSD solo se il sottocomando e "admin" e chi lancia e Admin
  char cur[32]; bool paused = false;
  if (berryAppRunning(cur, sizeof cur) && !strcmp(cur, c.driver)) {
    ErrBuf es;
    if (!berryStop(es)) { err = es.c_str(); return false; }
    paused = true;
  }
  vlog("CMD: %s %s %s (%s)", c.driver, c.cmd, c.sub, callerAdmin ? "admin" : "oper");   // mai gli argomenti: possono contenere percorsi o testi
  bool ok = berryRunCommand(code.c_str(), use, admin, call, out, err);
  if (paused) {
    ErrBuf er;
    if (!appStart(c.driver, er, ROLE_ADMIN, "comando")) vlog("APP: %s non riavviata dopo il comando: %s", c.driver, er.c_str());   // era gia in esecuzione: si ripristina con gli stessi diritti di prima
  }
  return ok;
}
#endif
