// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_diario.cpp
#include "vos_diario.h"
#include "vos_log.h"
#include <freertos/FreeRTOS.h>
#include "vos_i18n.h"
#include "vos_util.h"
#include "vos_sys.h"
#include <LittleFS.h>
#include <esp_system.h>
#include <time.h>

#ifndef RTC_NOINIT_ATTR
#define RTC_NOINIT_ATTR
#endif

#define DIARY_FILE "/boots3.bin"
#define DIARY_OLD  "/boots2.bin"
#define DIARY_OLD1 "/boots.bin"
#define HDR_MAGIC  0x56444941u
#define HDR_VER    2
#define DIARY_MAX  20
#define SNAP_MAGIC 0x56534E50u

struct RtcSnap { uint32_t magic, seq, epoch, upSec, minHeap, chk, mhz; char task[16]; char last[100]; char stage[12]; uint16_t maxBlockKb, stalls; char stuck[20]; };
struct BootRecV1 { uint32_t n, epoch, prevUp, minHeap, mhz; uint8_t reason, hasSnap, pad0, pad1; char task[16]; char last[100]; char stage[12]; };
struct BootRec { uint32_t n, epoch, prevUp, minHeap, mhz; uint8_t reason, hasSnap, pad0, pad1; char task[16]; char last[100]; char stage[12]; uint16_t maxBlockKb, stalls; char stuck[20]; };

struct DiaryHdr { uint32_t magic; uint16_t ver, recSize; };

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
// piu task scrivono l'istantanea (loop, monitor, registro): un blocco brevissimo evita un checksum che non corrisponde ai dati
static portMUX_TYPE g_snapMux = portMUX_INITIALIZER_UNLOCKED;
#define SNAP_LOCK()   portENTER_CRITICAL(&g_snapMux)
#define SNAP_UNLOCK() portEXIT_CRITICAL(&g_snapMux)

static void snapSeal() { g_snap.magic = SNAP_MAGIC; g_snap.chk = chkOf(g_snap); }

void diaryEarly() {
  g_reasonNow = (int)esp_reset_reason();
  g_oldOk = (g_snap.magic == SNAP_MAGIC && g_snap.chk == chkOf(g_snap));
  if (g_oldOk) memcpy(&g_old, &g_snap, sizeof(g_old));
  memset(&g_snap, 0, sizeof(g_snap));               // da qui si scrive l'istantanea di QUESTO avvio
  g_snap.seq = g_oldOk ? g_old.seq + 1 : 1;
  snapSeal();
}

void diaryStage(const char* stage) {
  if (!stage) return;
  SNAP_LOCK();
  strncpy(g_snap.stage, stage, sizeof(g_snap.stage) - 1); g_snap.stage[sizeof(g_snap.stage) - 1] = 0;
  snapSeal();
  SNAP_UNLOCK();
}

void diaryNote(const char* line) {
  if (!line) return;
  // niente righe che si ripetono a ogni avvio (allarme della seriale) o che parlano del riavvio precedente: coprirebbero quella vera
  if (strstr(line, "AUDIT: GIALLO") || strstr(line, "riavvio anomalo") || strstr(line, "ultima riga grave")) return;
  SNAP_LOCK();
  strncpy(g_snap.last, line, sizeof(g_snap.last) - 1); g_snap.last[sizeof(g_snap.last) - 1] = 0;
  snapSeal();
  SNAP_UNLOCK();
}

void diaryExtra(uint32_t maxBlock, uint32_t stalls, const char* stuck) {
  SNAP_LOCK();
  g_snap.maxBlockKb = (uint16_t)min<uint32_t>(maxBlock / 1024, 65535); g_snap.stalls = (uint16_t)min<uint32_t>(stalls, 65535);
  strncpy(g_snap.stuck, stuck ? stuck : "", sizeof(g_snap.stuck) - 1); g_snap.stuck[sizeof(g_snap.stuck) - 1] = 0;
  snapSeal();
  SNAP_UNLOCK();
}

void diaryTick(uint32_t upSec, uint32_t minHeap, const char* topTask, int topPct, uint32_t mhz) {
  (void)topPct;
  if (upSec > 15 && upSec % 10 != 0) return;      // ogni secondo nei primi 15 s (cosi un blocco all'avvio lascia traccia), poi ogni 10 s
  time_t now = time(NULL);
  SNAP_LOCK();
  g_snap.epoch = now > 1700000000 ? (uint32_t)now : 0;
  g_snap.upSec = upSec; g_snap.minHeap = minHeap; g_snap.mhz = mhz;
  strncpy(g_snap.task, topTask ? topTask : "", sizeof(g_snap.task) - 1); g_snap.task[sizeof(g_snap.task) - 1] = 0;
  snapSeal();
  SNAP_UNLOCK();
}

static void* bigAlloc(size_t n) { void* p = psramFound() ? ps_malloc(n) : nullptr; return p ? p : malloc(n); }   // PSRAM se c'e: la RAM interna e poca

static int loadAll(BootRec* r) {
  File f = LittleFS.open(DIARY_FILE, "r");
  if (!f) return 0;
  DiaryHdr h;
  if (f.read((uint8_t*)&h, sizeof(h)) != sizeof(h) || h.magic != HDR_MAGIC) { f.close(); return 0; }
  int n = 0;
  if (h.ver == HDR_VER && h.recSize == sizeof(BootRec)) {
    while (n < DIARY_MAX && f.read((uint8_t*)&r[n], sizeof(BootRec)) == sizeof(BootRec)) n++;
  } else if (h.ver == 1 && h.recSize == sizeof(BootRecV1)) {          // diario della versione precedente: si tiene e si completa
    BootRecV1 o;
    while (n < DIARY_MAX && f.read((uint8_t*)&o, sizeof(o)) == sizeof(o)) {
      memset(&r[n], 0, sizeof(BootRec));
      r[n].n = o.n; r[n].epoch = o.epoch; r[n].prevUp = o.prevUp; r[n].minHeap = o.minHeap; r[n].mhz = o.mhz;
      r[n].reason = o.reason; r[n].hasSnap = o.hasSnap;
      memcpy(r[n].task, o.task, sizeof(o.task)); memcpy(r[n].last, o.last, sizeof(o.last)); memcpy(r[n].stage, o.stage, sizeof(o.stage));
      n++;
    }
  }                                                                    // formato sconosciuto: si scarta
  f.close();
  return n;
}
static void saveAll(const BootRec* r, int n) {
  File f = LittleFS.open(DIARY_FILE, "w");
  if (!f) return;
  DiaryHdr h = { HDR_MAGIC, HDR_VER, (uint16_t)sizeof(BootRec) };
  f.write((const uint8_t*)&h, sizeof(h));
  f.write((const uint8_t*)r, n * sizeof(BootRec));
  f.close();
}

int diaryCount() {
  File f = LittleFS.open(DIARY_FILE, "r");
  if (!f) return 0;
  DiaryHdr h; size_t sz = f.size();
  bool okHdr = sz >= sizeof(DiaryHdr) && f.read((uint8_t*)&h, sizeof(h)) == sizeof(h) && h.magic == HDR_MAGIC && ((h.ver == HDR_VER && h.recSize == sizeof(BootRec)) || (h.ver == 1 && h.recSize == sizeof(BootRecV1)));
  f.close();
  if (!okHdr) return 0;                              // file vuoto, troncato o di un altro formato: come se non ci fosse
  int n = (int)((sz - sizeof(DiaryHdr)) / h.recSize);
  return n > DIARY_MAX ? DIARY_MAX : n;
}

static bool abnormal(int r) { return r == 4 || r == 5 || r == 6 || r == 7 || r == 9; }

void diaryInit() {
  LittleFS.remove(DIARY_OLD); LittleFS.remove(DIARY_OLD1);   // formati vecchi
  BootRec* r = (BootRec*)bigAlloc(sizeof(BootRec) * (DIARY_MAX + 1));
  if (!r) return;
  int n = loadAll(r);
  BootRec b; memset(&b, 0, sizeof(b));
  b.n = sysBootCount();
  b.reason = (uint8_t)g_reasonNow;
  if (g_oldOk && g_reasonNow != 1) {                 // dopo l'accensione l'istantanea non vale
    b.hasSnap = 1; b.mhz = g_old.mhz; b.epoch = g_old.epoch; b.prevUp = g_old.upSec; b.minHeap = g_old.minHeap;
    strncpy(b.task, g_old.task, sizeof(b.task) - 1); strncpy(b.last, g_old.last, sizeof(b.last) - 1); strncpy(b.stage, g_old.stage, sizeof(b.stage) - 1);
    b.maxBlockKb = g_old.maxBlockKb; b.stalls = g_old.stalls; strncpy(b.stuck, g_old.stuck, sizeof(b.stuck) - 1);
  }
  if (n >= DIARY_MAX) { memmove(r, r + 1, sizeof(BootRec) * (DIARY_MAX - 1)); n = DIARY_MAX - 1; }
  r[n++] = b;
  saveAll(r, n);
  free(r);
  vlog("AVVIO n.%lu: motivo %s, RAM libera %u KB, minima %u KB, %u MHz", (unsigned long)b.n, sysResetName(g_reasonNow).c_str(),
       (unsigned)(ESP.getFreeHeap() / 1024), (unsigned)(ESP.getMinFreeHeap() / 1024), (unsigned)getCpuFrequencyMhz());
  if (abnormal(g_reasonNow)) {
    if (b.hasSnap) vlog("ATTENZIONE: riavvio anomalo (%s). Prima era acceso da %lu s, fase %s, %lu MHz, RAM minima %lu KB, task %s", sysResetName(g_reasonNow).c_str(),
                        (unsigned long)b.prevUp, b.stage[0] ? b.stage : "-", (unsigned long)b.mhz, (unsigned long)(b.minHeap / 1024), b.task[0] ? b.task : "-");
    else vlog("ATTENZIONE: riavvio anomalo (%s), nessun dettaglio salvato", sysResetName(g_reasonNow).c_str());
    if (b.last[0]) vlog("ATTENZIONE: ultima riga grave prima del riavvio: %s", b.last);
  }
}

String diaryText(int maxN, bool withLast) {
  BootRec* r = (BootRec*)bigAlloc(sizeof(BootRec) * (DIARY_MAX + 1));
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
      if (b.stage[0]) o += "; " + trf("fase di avvio: %s", b.stage);
      if (b.mhz) o += "; " + trf("%u MHz", (unsigned)b.mhz);
      o += "\n";
      if (b.maxBlockKb || b.stalls || b.stuck[0]) {
        String x;
        if (b.maxBlockKb) x += trf("blocco RAM piu grande %u KB", (unsigned)b.maxBlockKb);
        if (b.stalls) { if (x.length()) x += "; "; x += trf("seriale non letta %u volte", (unsigned)b.stalls); }
        if (b.stuck[0]) { if (x.length()) x += "; "; x += trf("servizio in ritardo: %s", b.stuck); }
        o += "    " + x + "\n";
      }
      if (withLast && b.last[0]) o += "    " + trf("ultima riga grave: %s", b.last) + "\n";
    } else o += "    " + String(tr("nessun dettaglio prima del riavvio (accensione o prima registrazione)")) + "\n";
  }
  free(r);
  return o;
}

void diaryClear() { LittleFS.remove(DIARY_FILE); }
