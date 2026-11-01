// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_drvwidget.cpp (vedi vos_drvwidget.h): elenco, valori in cache e JSON dei widget dei driver.
#include "vos_drvwidget.h"
#if VOS_WITH_DRVCMD && VOS_WITH_BERRY
#include <stdio.h>
#include <string.h>
#include "../core/vos_lock.h"
#include "../drivers/vos_drv_time.h"
#include "../devices/vos_mod_cmd.h"
#if VOS_WITH_ELF
#include "vos_vde.h"
#endif

namespace {

struct Slot {
  ModWidgetEntry e;             // titolo, testo alternativo, ruolo, icona
  ModWidgetVals  vals;          // ultimo valore noto
  bool           have;          // vals e valido
  uint32_t       at;            // quando (ms) e stato letto
};

Slot* g_slot = nullptr;         // DRVW_MAX posti, sull'heap solo se c'e almeno un widget
int   g_n = 0;
bool  g_valid = false;
bool  g_running = false;        // un'altra richiesta sta gia chiamando on_widget()

VosMutex& mx() { static VosMutex m = [] { VosMutex v; v.init(); return v; }(); return m; }

// Rilegge /devs. Si chiama con il blocco preso.
void load() {
  if (g_valid) return;
  delete[] g_slot; g_slot = nullptr; g_n = 0;
  ModWidgetEntry* tmp = new ModWidgetEntry[DRVW_MAX];
  int n = modWidgetList(tmp, DRVW_MAX);
#if VOS_WITH_ELF
  if (n < DRVW_MAX) n += vdeWidgetLista(tmp + n, DRVW_MAX - n);          // anche i driver in ELF (.vde)
#endif
  if (n > 0) {
    g_slot = new Slot[n];
    memset(g_slot, 0, sizeof(Slot) * n);
    for (int i = 0; i < n; i++) g_slot[i].e = tmp[i];
    g_n = n;
  }
  delete[] tmp;
  g_valid = true;
}

// Chiama on_widget() del driver. Senza il blocco preso (puo durare fino a DRVW_RUN_MS). Ritorna true e riempie vals se ha risposto.
bool runWidget(const char* driver, ModWidgetVals& vals) {
  HeapBuf code(MOD_CMDCODE_MAX + 64); char use[100]; ErrBuf e;
  if (!modCmdCode(driver, code, use, sizeof use, e)) return false;
  code.add("\non_widget()\n");
  if (code.oom() || code.over()) return false;
  CmdCall* call = new CmdCall();
  memset(call, 0, sizeof *call);
  snprintf(call->sub, sizeof call->sub, "widget");
  HeapBuf out(BERRY_OUT_MAX + 64);
  bool ok = berryRunCommand(code.c_str(), use, false, *call, out, e, DRVW_RUN_MS);   // admin = false: il widget legge e basta
  delete call;
  if (!ok) return false;
  modWidgetParseOut(out.c_str(), vals);
  return true;
}

#if VOS_WITH_ELF
// Driver in ELF: i valori li da' il firmware (source=) o il modulo (argv[1] = "widget"); niente interprete Berry, quindi mai "occupato".
void refreshElf(int i, const char* driver, const char* source) {
  uint32_t now = drvMillis();
  {
    VosLock L(mx());
    if (g_running || (!source[0] && g_slot[i].have && now - g_slot[i].at < DRVW_CACHE_MS)) return;   // il valore del firmware e gia in memoria: si rilegge sempre
    g_running = true;
  }
  ModWidgetVals v;
  bool ok = vdeWidgetValori(driver, source, v);
  VosLock L(mx());
  g_running = false;
  if (ok && g_valid && i < g_n) { g_slot[i].vals = v; g_slot[i].have = true; g_slot[i].at = drvMillis(); }
}
#endif

// Se serve e si puo, aggiorna il valore del posto i. Senza il blocco preso.
void refreshIfStale(int i, const char* driver) {
  uint32_t now = drvMillis();
  {
    VosLock L(mx());
    if (g_running || (g_slot[i].have && now - g_slot[i].at < DRVW_CACHE_MS)) return;
    g_running = true;
  }
  ModWidgetVals v;
  bool ok = !berryRunning() && runWidget(driver, v);        // Berry occupato: si tiene il valore vecchio
  VosLock L(mx());
  g_running = false;
  if (ok && g_valid && i < g_n) { g_slot[i].vals = v; g_slot[i].have = true; g_slot[i].at = drvMillis(); }
}

}  // namespace

void drvWidgetRefresh() { VosLock L(mx()); g_valid = false; }

bool drvWidgetsJson(HeapBuf& out, int role) {
  if (role < ROLE_OPER) return out.add("[]");
  int n; char names[DRVW_MAX][25]; char sources[DRVW_MAX][13];
  { VosLock L(mx()); load(); n = g_n;
    for (int i = 0; i < n; i++) { snprintf(names[i], sizeof names[i], "%s", g_slot[i].e.driver); snprintf(sources[i], sizeof sources[i], "%s", g_slot[i].e.source); } }
  for (int i = 0; i < n; i++) {
#if VOS_WITH_ELF
    if (vdeEsiste(names[i])) { refreshElf(i, names[i], sources[i]); continue; }
#endif
    refreshIfStale(i, names[i]);
  }
  VosLock L(mx());
  if (!g_valid) return out.add("[]");                       // un driver e stato toccato proprio adesso: alla prossima richiesta
  out.add('[');
  bool first = true;
  for (int i = 0; i < g_n; i++) {
    const Slot& s = g_slot[i];
    if (!strcmp(s.e.w.who, "admin") && role < ROLE_ADMIN) continue;
    if (!first) out.add(',');
    first = false;
    out.add("{\"d\":\""); out.addJson(s.e.driver);
    out.add("\",\"t\":\""); out.addJson(s.e.w.title);
    out.add("\",\"a\":\""); out.addJson(s.e.w.alt);
    out.add("\",\"i\":\""); out.addJson(s.e.icon);
    out.add("\",\"st\":\""); out.add(s.have ? s.vals.st : "off");
    out.add("\",\"v\":\""); out.addJson(s.have ? s.vals.v : "-");
    out.add("\",\"s\":\""); out.addJson(s.have ? s.vals.s : "");
    out.add("\"}");
  }
  out.add(']');
  return !(out.oom() || out.over());
}
#endif
