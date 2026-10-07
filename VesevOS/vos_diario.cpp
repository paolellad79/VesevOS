// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_diario.cpp
#include "vos_diario.h"
#include "vos_log.h"
#include "vos_i18n.h"
#include "vos_util.h"
#include "vos_sys.h"
#include <LittleFS.h>
#include <esp_system.h>
#include <time.h>

#ifndef RTC_NOINIT_ATTR
#define RTC_NOINIT_ATTR
#endif

#define DIARY_FILE "/boots.bin"
#define DIARY_MAX  20
#define SNAP_MAGIC 0x56534E50u

struct RtcSnap { uint32_t magic, seq, epoch, upSec, minHeap, chk; char task[16]; char last[100]; };
struct BootRec { uint32_t n, epoch, prevUp, minHeap; uint8_t reason, hasSnap, pad0, pad1; char task[16]; char last[100]; };

RTC_NOINIT_ATTR static RtcSnap g_snap;
static RtcSnap g_old;
static bool    g_oldOk = false;
static int     g_reasonNow = 0;

static uint32_t chkOf(const RtcSnap& s) {
  uint32_t h = 2166136261u;
  const uint8_t* p = (const uint8_t*)&s;
  for (size_t i = 0; i < offsetof(RtcSnap, chk); i++) { h ^= p[i]; h *= 16777619u; }
  for (size_t i = sizeof(uint32_t) * 6; i < sizeof(RtcSnap); i++) { h ^= p[i]; h *= 16777619u; }
  return h;
}
static void snapSeal() { g_snap.magic = SNAP_MAGIC; g_snap.chk = chkOf(g_snap); }

void diaryEarly() {
  g_reasonNow = (int)esp_reset_reason();
  g_oldOk = (g_snap.magic == SNAP_MAGIC && g_snap.chk == chkOf(g_snap));
  if (g_oldOk) memcpy(&g_old, &g_snap, sizeof(g_old));
  memset(&g_snap, 0, sizeof(g_snap));               // da qui si scrive l'istantanea di QUESTO avvio
  g_snap.seq = g_oldOk ? g_old.seq + 1 : 1;
  snapSeal();
}

void diaryNote(const char* line) {
  if (!line) return;
  strncpy(g_snap.last, line, sizeof(g_snap.last) - 1); g_snap.last[sizeof(g_snap.last) - 1] = 0;
  snapSeal();
}

void diaryTick(uint32_t upSec, uint32_t minHeap, const char* topTask, int topPct) {
  (void)topPct;
  if (upSec % 10 != 0) return;
  time_t now = time(NULL);
  g_snap.epoch = now > 1700000000 ? (uint32_t)now : 0;
  g_snap.upSec = upSec; g_snap.minHeap = minHeap;
  strncpy(g_snap.task, topTask ? topTask : "", sizeof(g_snap.task) - 1); g_snap.task[sizeof(g_snap.task) - 1] = 0;
  snapSeal();
}

static int loadAll(BootRec* r) {
  File f = LittleFS.open(DIARY_FILE, "r");
  if (!f) return 0;
  int n = 0;
  while (n < DIARY_MAX && f.read((uint8_t*)&r[n], sizeof(BootRec)) == sizeof(BootRec)) n++;
  f.close();
  return n;
}
static void saveAll(const BootRec* r, int n) {
  File f = LittleFS.open(DIARY_FILE, "w");
  if (!f) return;
  f.write((const uint8_t*)r, n * sizeof(BootRec));
  f.close();
}

int diaryCount() {
  File f = LittleFS.open(DIARY_FILE, "r");
  if (!f) return 0;
  int n = (int)(f.size() / sizeof(BootRec)); f.close();
  return n > DIARY_MAX ? DIARY_MAX : n;
}

static bool abnormal(int r) { return r == 4 || r == 5 || r == 6 || r == 7 || r == 9; }

void diaryInit() {
  BootRec* r = (BootRec*)malloc(sizeof(BootRec) * (DIARY_MAX + 1));
  if (!r) return;
  int n = loadAll(r);
  BootRec b; memset(&b, 0, sizeof(b));
  b.n = sysBootCount();
  b.reason = (uint8_t)g_reasonNow;
  if (g_oldOk && g_reasonNow != 1) {                 // dopo l'accensione l'istantanea non vale
    b.hasSnap = 1; b.epoch = g_old.epoch; b.prevUp = g_old.upSec; b.minHeap = g_old.minHeap;
    strncpy(b.task, g_old.task, sizeof(b.task) - 1); strncpy(b.last, g_old.last, sizeof(b.last) - 1);
  }
  if (n >= DIARY_MAX) { memmove(r, r + 1, sizeof(BootRec) * (DIARY_MAX - 1)); n = DIARY_MAX - 1; }
  r[n++] = b;
  saveAll(r, n);
  free(r);
  vlog("AVVIO n.%lu: motivo %s, RAM libera %u KB, minima %u KB, %u MHz", (unsigned long)b.n, sysResetName(g_reasonNow).c_str(),
       (unsigned)(ESP.getFreeHeap() / 1024), (unsigned)(ESP.getMinFreeHeap() / 1024), (unsigned)getCpuFrequencyMhz());
  if (abnormal(g_reasonNow)) {
    if (b.hasSnap) vlog("ATTENZIONE: riavvio anomalo (%s). Prima era acceso da %lu s, RAM minima %lu KB, task piu attivo: %s", sysResetName(g_reasonNow).c_str(),
                        (unsigned long)b.prevUp, (unsigned long)(b.minHeap / 1024), b.task[0] ? b.task : "-");
    else vlog("ATTENZIONE: riavvio anomalo (%s), nessun dettaglio salvato", sysResetName(g_reasonNow).c_str());
    if (b.last[0]) vlog("ATTENZIONE: ultima riga grave prima del riavvio: %s", b.last);
  }
}

String diaryText(int maxN, bool withLast) {
  BootRec* r = (BootRec*)malloc(sizeof(BootRec) * (DIARY_MAX + 1));
  if (!r) return "";
  int n = loadAll(r);
  String o;
  for (int i = n - 1, c = 0; i >= 0 && c < maxN; i--, c++) {
    const BootRec& b = r[i];
    String when;
    if (b.epoch) { time_t t = (time_t)b.epoch; struct tm tmv; localtime_r(&t, &tmv); char s[24]; strftime(s, sizeof(s), "%d/%m %H:%M:%S", &tmv); when = s; }
    else when = tr("ora non nota");
    o += trf("Avvio n.%u - %s - %s", (unsigned)b.n, when.c_str(), sysResetName(b.reason).c_str());
    if (abnormal(b.reason)) o += " (!)";
    o += "\n";
    if (b.hasSnap) {
      o += "    " + trf("era acceso da %s, RAM minima %u KB", uptimeStr(b.prevUp).c_str(), (unsigned)(b.minHeap / 1024));
      if (b.task[0]) o += "; " + trf("task piu attivo: %s", b.task);
      o += "\n";
      if (withLast && b.last[0]) o += "    " + trf("ultima riga grave: %s", b.last) + "\n";
    } else o += "    " + String(tr("nessun dettaglio prima del riavvio (accensione o prima registrazione)")) + "\n";
  }
  free(r);
  return o;
}

void diaryClear() { LittleFS.remove(DIARY_FILE); }
