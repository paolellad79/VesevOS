// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_log.cpp
#include "vos_log.h"
#include "vos_serial.h"
#include "vos_util.h"
#include "vos_time.h"
#include "vos_diario.h"
#include "vos_ramstorico.h"
#include <stdarg.h>
#include <time.h>
#include "../drivers/vos_drv_time.h"
#include "../drivers/vos_drv_heap.h"
#include "../core/vos_clamp.h"
#include "../boards/vos_board.h"

#define LOG_LINES VOS_LOG_RIGHE   // 150 (C3: 40), vedi boards/vos_board.h
#define LOG_LEN   120
#define Q_BOOT    0x80000000u   // bit alto: il valore e "secondi dall'avvio", non un'epoca
#define STAMP_MAX TIME_FMT_MAX   // data e ora al massimo (formato scritto a mano compreso, tagliato se piu lungo)

// il registro (18 KB, C3: 4,8 KB) sta nella PSRAM se c'e: cosi la RAM interna resta libera per Wi-Fi e connessioni cifrate
static char     (*g_lines)[LOG_LEN] = nullptr;
static uint32_t g_q[LOG_LINES];          // quando: epoca; con il bit alto acceso = secondi dall'avvio (ora non valida). 4 byte per riga
static uint16_t g_head = 0;
static uint16_t g_count = 0;
static volatile int g_level = LG_INFO;
static SemaphoreHandle_t g_mx = NULL;

void logInit() {
  if (!g_lines) {
    size_t n = (size_t)LOG_LINES * LOG_LEN;
    void* p = drvPsramMalloc(n);
    if (!p) p = malloc(n);
    if (p) { memset(p, 0, n); g_lines = (char (*)[LOG_LEN])p; }
  }
  if (!g_mx) g_mx = xSemaphoreCreateMutex();
}

void logSetLevel(int lv) { g_level = vosClamp(lv, 0, 3); }
int  logLevel() { return g_level; }

// il livello si decide dall'INIZIO della riga (e un testo scelto dall'utente, come il nome della scheda, non puo alzarlo)
static int autoLevel(const char* b) {
  if (!strncmp(b, "ERRORE", 6) || !strncmp(b, "AUDIT: ROSSO", 12)) return LG_ERR;
  if (!strncmp(b, "ATTENZIONE", 10) || !strncmp(b, "AUDIT: GIALLO", 13)) return LG_WARN;
  return LG_INFO;
}

static void put(int lv, const char* buf) {
  if (lv > g_level) return;
  serLogLine(buf);
  if (lv <= LG_WARN) diaryNote(buf);
  if (!g_mx || !g_lines) return;
  if (xSemaphoreTake(g_mx, pdMS_TO_TICKS(50)) == pdTRUE) {
    time_t now = time(NULL);
    // si salva solo il numero: il formato di data e ora si applica quando si mostra la riga
    g_q[g_head] = now > 1700000000 ? (uint32_t)now : (Q_BOOT | (uint32_t)(drvMillis() / 1000));
    static const char TAG[4] = {'E', 'W', 'I', 'D'};
    snprintf(g_lines[g_head], LOG_LEN, "[%c] %s", TAG[lv & 3], buf);
    g_head = (g_head + 1) % LOG_LINES;
    if (g_count < LOG_LINES) g_count++;
    xSemaphoreGive(g_mx);
  }
}

static void fmtClean(char* buf, size_t n, const char* fmt, va_list ap) {
  vsnprintf(buf, n, fmt, ap);
  for (size_t i = 0; buf[i]; i++) {                 // pulizia: solo ASCII stampabile
    unsigned char c = (unsigned char)buf[i];
    if (c < 32 || c >= 127) buf[i] = '?';
  }
}

void vlog(const char* fmt, ...) {
  char buf[LOG_LEN - 4];
  va_list ap; va_start(ap, fmt); fmtClean(buf, sizeof(buf), fmt, ap); va_end(ap);
  put(autoLevel(buf), buf);
}

void vlogl(int lv, const char* fmt, ...) {
  char buf[LOG_LEN - 4];
  va_list ap; va_start(ap, fmt); fmtClean(buf, sizeof(buf), fmt, ap); va_end(ap);
  put(vosClamp(lv, 0, 3), buf);
}

// Tempo di una riga come testo: data e ora nel formato scelto, oppure "[Ns]" se l'ora non era valida
static void whenText(uint32_t q, char* out, size_t n) {
  if (q & Q_BOOT) { snprintf(out, n, "[%lus]", (unsigned long)(q & ~Q_BOOT)); return; }
  timeFmtEpoch(q, true, out, n);
}

// raw = false: "<data ora> [I] testo"; raw = true (per la pagina): "<epoca> [I] testo" oppure "u<secondi> [I] testo"
static std::string logRead(int maxLines, bool raw) {
  std::string r;
  if (!g_mx || !g_lines) return r;
  if (xSemaphoreTake(g_mx, pdMS_TO_TICKS(100)) != pdTRUE) return r;
  int n = g_count < maxLines ? g_count : maxLines;
  int start = (g_head - n + LOG_LINES * 2) % LOG_LINES;
  size_t total = 0;                                   // prima si misura, poi si prende la memoria una volta sola
  for (int i = 0; i < n; i++) total += strlen(g_lines[(start + i) % LOG_LINES]) + STAMP_MAX + 2;
  r.reserve(total);
  for (int i = 0; i < n; i++) {
    int k = (start + i) % LOG_LINES;
    char when[STAMP_MAX];
    if (!raw) whenText(g_q[k], when, sizeof when);
    else if (g_q[k] & Q_BOOT) snprintf(when, sizeof when, "u%lu", (unsigned long)(g_q[k] & ~Q_BOOT));
    else snprintf(when, sizeof when, "%lu", (unsigned long)g_q[k]);
    r.append(when);
    r.push_back(' ');
    r.append(g_lines[k]);
    r.push_back('\n');
  }
  xSemaphoreGive(g_mx);
  return r;
}

std::string logGet(int maxLines) { return logRead(maxLines, false); }
std::string logGetRaw(int maxLines) { return logRead(maxLines, true); }

int logCount() { return g_count; }

void logClear() {
  if (!g_mx) return;
  if (xSemaphoreTake(g_mx, pdMS_TO_TICKS(100)) != pdTRUE) return;
  g_head = 0; g_count = 0;
  xSemaphoreGive(g_mx);
  vlog("LOG: registro svuotato");
}

void ramTrace(const char* fase) {
  ramNote(fase);
  vlog("RAM: %s libera %u KB blocco massimo %u KB", fase ? fase : "-", (unsigned)(drvHeapFree() / 1024), (unsigned)(drvHeapMaxBlock() / 1024));
}
