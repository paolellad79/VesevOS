// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_sys_task.cpp
// Elenco dei task (testo e JSON) e arresto/riavvio dei task della lista consentita. Scritto su HeapBuf, senza String.
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "vos_sys.h"
#include "vos_i18n.h"
#include "vos_log.h"

// Tipo: 0 = sistema (protetto), 1 = VesevOS, 2 = app (futuro)
// Si possono fermare solo i task della lista qui sotto (lista consentita, non lista dei vietati).
// Lista consentita: i moduli registrano i loro task con sysTaskRegister (nome + funzione che lo avvia).
struct TaskDef { const char* n; SysTaskStart f; SysTaskStart stop; };
static TaskDef g_defs[12]; static int g_ndefs = 0;

void sysTaskRegister(const char* name, SysTaskStart start, SysTaskStart stop) {
  for (int i = 0; i < g_ndefs; i++) if (strcmp(g_defs[i].n, name) == 0) { g_defs[i].f = start; g_defs[i].stop = stop; return; }
  if (g_ndefs < 12) { g_defs[g_ndefs].n = name; g_defs[g_ndefs].f = start; g_defs[g_ndefs].stop = stop; g_ndefs++; }
}
static int defOf(const char* name) {
  for (int i = 0; i < g_ndefs; i++) if (strcmp(g_defs[i].n, name) == 0) return i;
  return -1;
}
bool sysTaskRunning(const char* name) { return xTaskGetHandle(name) != NULL; }

// Scrive un testo formattato (printf) nel HeapBuf: i pezzi di questo file sono tutti corti.
static bool addFmt(HeapBuf& o, const char* fmt, ...) __attribute__((format(printf, 2, 3)));
static bool addFmt(HeapBuf& o, const char* fmt, ...) {
  char t[200];
  va_list ap; va_start(ap, fmt);
  int n = vsnprintf(t, sizeof(t), fmt, ap);
  va_end(ap);
  if (n < 0) return true;
  return o.add(t, n < (int)sizeof(t) ? (size_t)n : sizeof(t) - 1);
}

bool sysTasksText(HeapBuf& o) {
  bool ok = addFmt(o, tr("Task attivi: %u"), (unsigned)uxTaskGetNumberOfTasks()) && o.add('\n');
#if (configUSE_TRACE_FACILITY == 1) && (configUSE_STATS_FORMATTING_FUNCTIONS == 1)
  static char buf[1024];
  vTaskList(buf);
  ok = ok && o.add("Nome          Stato Prio Stack Num\n");
  // come cleanAscii: via gli a-capo di Windows, i caratteri strani diventano '?'
  for (const char* p = buf; *p && ok; p++) {
    unsigned char c = (unsigned char)*p;
    if (c == '\r') continue;
    ok = (c == '\n' || c == '\t' || (c >= 32 && c < 127)) ? o.add((char)c) : o.add('?');
  }
#else
  ok = ok && addFmt(o, "%s\n", tr("(dettagli non disponibili in questa build)"));
#endif
  return ok;
}

#if (configUSE_TRACE_FACILITY == 1)          // usati solo qui sotto: fuori da questo blocco darebbero l'avviso "non usata"
static const char* const OURS[] = {"led", "time", "monitor", "net", "rules", "mqtt", "mesh", "wd"};
static bool inList(const char* name, const char* const* list, int n) {
  for (int i = 0; i < n; i++) if (strcmp(name, list[i]) == 0) return true;
  return false;
}
#endif

// JSON dell'elenco. Se manca la memoria o si supera il tetto ritorna false (il chiamante non manda un JSON troncato).
bool sysTasksJson(HeapBuf& j) {
#if (configUSE_TRACE_FACILITY == 1)
  UBaseType_t n = uxTaskGetNumberOfTasks();
  TaskStatus_t* a = (TaskStatus_t*)malloc((n + 2) * sizeof(TaskStatus_t));
  if (!a) return j.add("{\"ok\":false}");
  uint32_t total = 0;
  n = uxTaskGetSystemState(a, n + 2, &total);
  bool ok = addFmt(j, "{\"ok\":true,\"total\":%lu,\"tasks\":[", (unsigned long)total);
  for (UBaseType_t i = 0; i < n && ok; i++) {
    const char* nm = a[i].pcTaskName;
    int type = inList(nm, OURS, 8) ? 1 : 0;
    bool kill = defOf(nm) >= 0;
    if (i) ok = j.add(',');
    ok = ok && j.add("{\"n\":\"") && j.addJson(nm, strlen(nm));
    ok = ok && addFmt(j, "\",\"id\":%u,\"s\":%d,\"p\":%u,\"stk\":%lu,\"rt\":%lu,\"t\":%d,\"k\":%s}",
                      (unsigned)a[i].xTaskNumber, (int)a[i].eCurrentState, (unsigned)a[i].uxCurrentPriority,
                      (unsigned long)a[i].usStackHighWaterMark, (unsigned long)a[i].ulRunTimeCounter, type, kill ? "true" : "false");
  }
  free(a);
  // task registrati ma fermati: compaiono con stato 9 (fermato) per poterli riavviare
  for (int d = 0; d < g_ndefs && ok; d++) {
    if (xTaskGetHandle(g_defs[d].n)) continue;
    if (n) ok = j.add(',');
    n++;
    ok = ok && addFmt(j, "{\"n\":\"%s\",\"id\":0,\"s\":9,\"p\":0,\"stk\":0,\"rt\":0,\"t\":1,\"k\":true}", g_defs[d].n);
  }
  return ok && j.add("]}");
#else
  return j.add("{\"ok\":false}");
#endif
}

static void setErr(char* err, size_t en, const char* msg) {
  if (err && en) { strncpy(err, msg, en - 1); err[en - 1] = 0; }
}

bool sysTaskKill(const char* name, char* err, size_t en) {
  size_t len = strlen(name);
  if (len == 0 || len > 16) { setErr(err, en, tr("Nome task non valido")); return false; }
  int d = defOf(name);
  if (d < 0) { setErr(err, en, tr("Questo task e protetto: non si puo fermare")); return false; }
  TaskHandle_t h = xTaskGetHandle(name);
  if (!h) { setErr(err, en, tr("Task non trovato")); return false; }
  vlog("TASK: fermato '%s' su richiesta", name);
  if (g_defs[d].stop) g_defs[d].stop(); else vTaskDelete(h);
  return true;
}

bool sysTaskRestart(const char* name, char* err, size_t en) {
  size_t len = strlen(name);
  if (len == 0 || len > 16) { setErr(err, en, tr("Nome task non valido")); return false; }
  int d = defOf(name);
  if (d < 0) { setErr(err, en, tr("Questo task non si puo riavviare")); return false; }
  TaskHandle_t h = xTaskGetHandle(name);
  if (h) { if (g_defs[d].stop) g_defs[d].stop(); else vTaskDelete(h); vTaskDelay(pdMS_TO_TICKS(50)); }
  g_defs[d].f();
  vlog("TASK: %s '%s' su richiesta", h ? "riavviato" : "avviato", name);
  return true;
}
