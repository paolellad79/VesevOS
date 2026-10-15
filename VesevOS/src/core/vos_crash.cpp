// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_crash.cpp
#include "vos_crash.h"
#include "vos_common.h"
#include "vos_log.h"
#include "vos_i18n.h"
#include "vos_time.h"
#include "../drivers/vos_drv_fs.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_system.h>

#ifndef RTC_NOINIT_ATTR
#define RTC_NOINIT_ATTR
#endif

#define CRASH_MAGIC 0x56435253u      // "VCRS"
#define CRASH_BT    16
#define CRASH_FILE  "/crash.txt"
#define CRASH_KEEP  5

struct CrashRtc { uint32_t magic, chk; int32_t core; uint32_t pc, n; uint32_t bt[CRASH_BT]; uint8_t corrupt, more, pad0, pad1; char reason[48]; char task[16]; char ver[16]; };
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

// Chiamato DENTRO il crash: niente memoria dinamica, niente file, niente blocchi. Solo copie nella memoria RTC.
static void onPanic(arduino_panic_info_t* info, void*) {
  if (!info) return;
  memset(&r_crash, 0, sizeof(r_crash));
  r_crash.core = info->core;
  r_crash.pc = (uint32_t)(uintptr_t)info->pc;
  r_crash.n = info->backtrace_len < CRASH_BT ? info->backtrace_len : CRASH_BT;
  for (uint32_t i = 0; i < r_crash.n; i++) r_crash.bt[i] = info->backtrace[i];
  r_crash.corrupt = info->backtrace_corrupt; r_crash.more = info->backtrace_continues || info->backtrace_len > CRASH_BT;
  cpy(r_crash.reason, sizeof(r_crash.reason), info->reason);
  TaskHandle_t t = xTaskGetCurrentTaskHandle();
  cpy(r_crash.task, sizeof(r_crash.task), t ? pcTaskGetName(t) : "");
  cpy(r_crash.ver, sizeof(r_crash.ver), VOS_VERSION);
  r_crash.magic = CRASH_MAGIC; r_crash.chk = chkOf(r_crash);
}

void crashEarly() {
  g_prevOk = r_crash.magic == CRASH_MAGIC && r_crash.chk == chkOf(r_crash);
  if (g_prevOk) memcpy(&g_prev, &r_crash, sizeof(g_prev));
  // il core 3.3.x segnala il panico del Task watchdog come "Syscall": il motivo vero e il reset
  if (g_prevOk && esp_reset_reason() == ESP_RST_TASK_WDT) cpy(g_prev.reason, sizeof(g_prev.reason), "Task watchdog");
  memset(&r_crash, 0, sizeof(r_crash));               // un rapporto si legge una volta sola
}

void crashInstall() { set_arduino_panic_handler(onPanic, nullptr); }

static String reportOf(const CrashRtc& c) {
  char h[72];
  String r = timeNowStr() + "  VesevOS " + c.ver + "  " + c.reason + "\n";
  snprintf(h, sizeof(h), "  core %ld, task %s, PC 0x%08lx", (long)c.core, c.task[0] ? c.task : "?", (unsigned long)c.pc);
  r += h; r += "\n  Backtrace:";
  for (uint32_t i = 0; i < c.n && i < CRASH_BT; i++) { snprintf(h, sizeof(h), " 0x%08lx", (unsigned long)c.bt[i]); r += h; }
  if (c.more) r += " ...";
  if (c.corrupt) r += " (rovinato)";
  return r + "\n";
}

void crashBoot() {
  if (!g_prevOk) return;
  g_pending = true;
  String rep = reportOf(g_prev);
  vlog("ERRORE: crash prima del riavvio: %s (task %s, PC 0x%08lx). Dettagli: comando crash", g_prev.reason, g_prev.task[0] ? g_prev.task : "?", (unsigned long)g_prev.pc);
  // il piu recente in cima; si tengono gli ultimi CRASH_KEEP rapporti (ognuno finisce con una riga vuota)
  String old;
  File f = drvFsOpen(CRASH_FILE, "r");
  if (f) { old = f.readString(); f.close(); }
  String all = rep + "\n" + old;
  int cnt = 0, cut = -1;
  for (int i = 0; i + 1 < (int)all.length(); i++) if (all[i] == '\n' && all[i + 1] == '\n') { if (++cnt == CRASH_KEEP) { cut = i + 2; break; } }
  if (cut > 0) all = all.substring(0, cut);
  f = drvFsOpen(CRASH_FILE, "w");
  if (f) { f.print(all); f.close(); }
}

String crashText() {
  g_pending = false;
  File f = drvFsOpen(CRASH_FILE, "r");
  String t = f ? f.readString() : String("");
  if (f) f.close();
  if (!t.length()) return String(tr("Nessun crash registrato")) + "\n";
  return t + tr("Per file e riga: xtensa-esp32s3-elf-addr2line -pfiaC -e VesevOS.ino.elf <indirizzi> (con il .elf della stessa versione)") + "\n";
}

bool crashClear() { g_pending = false; return drvFsRemove(CRASH_FILE) || !drvFsExists(CRASH_FILE); }
bool crashPending() { return g_pending; }
