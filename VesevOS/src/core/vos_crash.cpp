// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_crash.cpp
#include "vos_crash.h"
#include "vos_common.h"
#include "vos_log.h"
#include "vos_strbuf.h"
#include "vos_i18n.h"
#include "vos_time.h"
#include "vos_layout.h"
#include "../drivers/vos_drv_fs.h"
#include "../drivers/vos_drv_panic.h"
#include "vos_diag_rule.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_system.h>

#ifndef RTC_NOINIT_ATTR
#define RTC_NOINIT_ATTR
#endif

#define CRASH_MAGIC 0x56435253u      // "VCRS"
#define CRASH_BT    16
#define CRASH_FILE  layoutPath(LAY_CRASH)    // /var/crash.txt (prima della copia /crash.txt)
#define CRASH_KEEP  5

struct CrashRtc { uint32_t magic, chk; int32_t core; uint32_t pc, n; uint32_t bt[CRASH_BT]; uint8_t corrupt, more, diagClasse, diagDett; char reason[48]; char task[16]; char ver[16]; };
RTC_NOINIT_ATTR static CrashRtc r_crash;
static CrashRtc g_prev;
static bool g_prevOk = false, g_pending = false;

static uint32_t chkOf(const CrashRtc& c) {
  uint32_t h = 2166136261u;
  const uint8_t* p = (const uint8_t*)&c;
  for (size_t i = sizeof(uint32_t) * 2; i < sizeof(CrashRtc); i++) { h ^= p[i]; h *= 16777619u; }
  return h;
}

static void cpy(char* d, size_t n, const char* s) { size_t i = 0; if (s) for (; s[i] && i + 1 < n; i++) d[i] = (s[i] >= 32 && s[i] < 127) ? s[i] : '_'; d[i] = 0; }

// Un giro compatto del codice sul LED, DENTRO il panico (solo C3 e C6, vedi vos_board.h). Al massimo DIAG_PANICO_MAX_MS (circa 4 s) per le classi 2-4:
// ben sotto i 10 s del watchdog RTC che il gestore del panico arma prima di chiamarci. Niente memoria, log, task o timer: solo il GPIO e l'attesa a cicli.
static void ledPanico(DiagCodice dc) {
#if VOS_LED_DIAG_PANICO
  int n = diagNumPassi(dc);
  if (dc.classe < DC_CPU || dc.classe > DC_MEMORIA) n = 0;     // solo le classi 2, 3, 4 (CPU, watchdog, memoria): sono quelle del panico
  for (int i = 0; i < n; i++) {
    DiagPasso p = diagPasso(dc, i, false);
    drvPanicLed(true);
    drvPanicAttendiMs(p.acceso);
    drvPanicLed(false);
    drvPanicAttendiMs(p.spento);
  }
#else
  (void)dc;
#endif
}

// Chiamato DENTRO il crash: niente memoria dinamica, niente file, niente blocchi. Solo copie nella memoria RTC.
static void onPanic(const DrvPanicInfo* info) {
  if (!info) return;
  memset(&r_crash, 0, sizeof(r_crash));
  r_crash.core = info->core;
  r_crash.pc = (uint32_t)(uintptr_t)info->pc;
  r_crash.n = info->backtraceLen < CRASH_BT ? info->backtraceLen : CRASH_BT;
  for (uint32_t i = 0; i < r_crash.n; i++) r_crash.bt[i] = info->backtrace[i];
  r_crash.corrupt = info->backtraceCorrupt; r_crash.more = info->backtraceContinues || info->backtraceLen > CRASH_BT;
  cpy(r_crash.reason, sizeof(r_crash.reason), info->reason);
  DiagCodice dc = diagDaPanico(info->tipo, info->reason, info->segni, info->dettagli);     // classe e dettaglio per il LED (stesso posto dei due byte che prima erano vuoti)
  r_crash.diagClasse = dc.classe;
  r_crash.diagDett = dc.dett;
  TaskHandle_t t = xTaskGetCurrentTaskHandle();
  cpy(r_crash.task, sizeof(r_crash.task), t ? pcTaskGetName(t) : "");
  cpy(r_crash.ver, sizeof(r_crash.ver), VOS_VERSION);
  r_crash.magic = CRASH_MAGIC; r_crash.chk = chkOf(r_crash);
  ledPanico(dc);
}

void crashEarly() {
  g_prevOk = r_crash.magic == CRASH_MAGIC && r_crash.chk == chkOf(r_crash);
  if (g_prevOk) memcpy(&g_prev, &r_crash, sizeof(g_prev));
  // il core 3.3.x segnala il panico del Task watchdog come "Syscall": il motivo vero e il reset
  if (g_prevOk && esp_reset_reason() == ESP_RST_TASK_WDT) cpy(g_prev.reason, sizeof(g_prev.reason), "Task watchdog");
  memset(&r_crash, 0, sizeof(r_crash));               // un rapporto si legge una volta sola
}

void crashInstall() { drvPanicHook(onPanic); }

// Codice (classe, dettaglio) che il gancio aveva salvato prima dell'ultimo riavvio; 0,0 se non c'e un rapporto valido.
void crashPrevDiag(uint8_t* classe, uint8_t* dett) {
  *classe = g_prevOk ? g_prev.diagClasse : 0;
  *dett = g_prevOk ? g_prev.diagDett : 0;
}

// Un rapporto sta in meno di 400 byte (data, versione, motivo, riga del task, 16 indirizzi): un solo blocco di heap, liberato subito.
static std::string reportOf(const CrashRtc& c) {
  StrText t(448);
  StrBuf& r = t.buf();
  char now[TIME_FMT_MAX]; timeNowFmt(now, sizeof now);
  r.add(now).add("  VesevOS ").add(c.ver).add("  ").add(c.reason).add('\n');
  r.addf("  core %ld, task %s, PC 0x%08lx", (long)c.core, c.task[0] ? c.task : "?", (unsigned long)c.pc);
  r.add("\n  Backtrace:");
  for (uint32_t i = 0; i < c.n && i < CRASH_BT; i++) r.addf(" 0x%08lx", (unsigned long)c.bt[i]);
  if (c.more) r.add(" ...");
  if (c.corrupt) r.add(" (rovinato)");
  r.add('\n');
  return t.take();
}

// Aggiunge in coda a out il contenuto del file (al massimo CRASH_READ_MAX byte: 5 rapporti da meno di 450 byte).
#define CRASH_READ_MAX 4096
static void appendFile(const char* path, std::string& out) {
  VosFile f = drvFsOpen(path, "r");
  if (!f) return;
  char b[256]; int n; size_t total = 0;
  while (total < CRASH_READ_MAX && (n = f.read((uint8_t*)b, sizeof b)) > 0) { out.append(b, (size_t)n); total += (size_t)n; }
  f.close();
}

void crashBoot() {
  if (!g_prevOk) return;
  g_pending = true;
  std::string all = reportOf(g_prev);
  vlog("ERRORE: crash prima del riavvio: %s (task %s, PC 0x%08lx). Dettagli: comando crash", g_prev.reason, g_prev.task[0] ? g_prev.task : "?", (unsigned long)g_prev.pc);
  // il piu recente in cima; si tengono gli ultimi CRASH_KEEP rapporti (ognuno finisce con una riga vuota)
  all += '\n';
  appendFile(CRASH_FILE, all);
  int cnt = 0;
  for (size_t i = 0; i + 1 < all.size(); i++) {
    if (all[i] == '\n' && all[i + 1] == '\n' && ++cnt == CRASH_KEEP) { all.resize(i + 2); break; }
  }
  VosFile f = drvFsOpen(CRASH_FILE, "w");
  if (f) { f.write((const uint8_t*)all.data(), all.size()); f.close(); }
}

std::string crashText() {
  g_pending = false;
  std::string t;
  appendFile(CRASH_FILE, t);
  if (t.empty()) return std::string(tr("Nessun crash registrato")) + "\n";
  t += tr("Per file e riga: xtensa-esp32s3-elf-addr2line -pfiaC -e VesevOS.elf <indirizzi> (con il .elf della stessa versione)");
  t += '\n';
  return t;
}

bool crashClear() { g_pending = false; return drvFsRemove(CRASH_FILE) || !drvFsExists(CRASH_FILE); }
uint32_t crashFileBytes() {
  VosFile f = drvFsOpen(CRASH_FILE, "r");
  return (f && !f.isDirectory()) ? (uint32_t)f.size() : 0;
}
bool crashPending() { return g_pending; }
