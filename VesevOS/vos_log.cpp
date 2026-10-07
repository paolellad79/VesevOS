// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_log.cpp
#include "vos_log.h"
#include "vos_serial.h"
#include "vos_util.h"
#include "vos_diario.h"
#include <stdarg.h>
#include <time.h>

#define LOG_LINES 150
#define LOG_LEN   120

// il registro (18 KB) sta nella PSRAM se c'e: cosi la RAM interna resta libera per Wi-Fi e connessioni cifrate
static char     (*g_lines)[LOG_LEN] = nullptr;
static uint16_t g_head = 0;
static uint16_t g_count = 0;
static volatile int g_level = LG_INFO;
static SemaphoreHandle_t g_mx = NULL;

void logInit() {
  if (!g_lines) {
    size_t n = (size_t)LOG_LINES * LOG_LEN;
    void* p = psramFound() ? ps_malloc(n) : nullptr;
    if (!p) p = malloc(n);
    if (p) { memset(p, 0, n); g_lines = (char (*)[LOG_LEN])p; }
  }
  if (!g_mx) g_mx = xSemaphoreCreateMutex();
}

void logSetLevel(int lv) { g_level = constrain(lv, 0, 3); }
int  logLevel() { return g_level; }

static int autoLevel(const char* b) {
  if (strstr(b, "ERRORE") || strstr(b, "AUDIT: ROSSO")) return LG_ERR;
  if (strstr(b, "ATTENZIONE") || strstr(b, "AUDIT: GIALLO")) return LG_WARN;
  return LG_INFO;
}

static void put(int lv, const char* buf) {
  if (lv > g_level) return;
  serLogLine(buf);
  if (lv <= LG_WARN) diaryNote(buf);
  if (!g_mx || !g_lines) return;
  if (xSemaphoreTake(g_mx, pdMS_TO_TICKS(50)) == pdTRUE) {
    char stamp[24];
    time_t now = time(NULL);
    if (now > 1700000000) {                       // ora valida: data e ora vere
      struct tm tmv; localtime_r(&now, &tmv);
      strftime(stamp, sizeof(stamp), "%d/%m %H:%M:%S ", &tmv);
    } else snprintf(stamp, sizeof(stamp), "[%lus] ", (unsigned long)(millis() / 1000));
    static const char TAG[4] = {'E', 'W', 'I', 'D'};
    snprintf(g_lines[g_head], LOG_LEN, "%s[%c] %s", stamp, TAG[lv & 3], buf);
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
  put(constrain(lv, 0, 3), buf);
}

String logGet(int maxLines) {
  String r;
  if (!g_mx || !g_lines) return r;
  if (xSemaphoreTake(g_mx, pdMS_TO_TICKS(100)) != pdTRUE) return r;
  int n = g_count < maxLines ? g_count : maxLines;
  int start = (g_head - n + LOG_LINES * 2) % LOG_LINES;
  for (int i = 0; i < n; i++) {
    r += g_lines[(start + i) % LOG_LINES];
    r += '\n';
  }
  xSemaphoreGive(g_mx);
  return r;
}

void logClear() {
  if (!g_mx) return;
  if (xSemaphoreTake(g_mx, pdMS_TO_TICKS(100)) != pdTRUE) return;
  g_head = 0; g_count = 0;
  xSemaphoreGive(g_mx);
  vlog("LOG: registro svuotato");
}
