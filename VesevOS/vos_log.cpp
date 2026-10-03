// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_log.cpp
#include "vos_log.h"
#include "vos_util.h"
#include <stdarg.h>
#include <time.h>

#define LOG_LINES 150
#define LOG_LEN   120

static char     g_lines[LOG_LINES][LOG_LEN];
static uint16_t g_head = 0;
static uint16_t g_count = 0;
static SemaphoreHandle_t g_mx = NULL;

void logInit() {
  if (!g_mx) g_mx = xSemaphoreCreateMutex();
}

void vlog(const char* fmt, ...) {
  char buf[LOG_LEN];
  va_list ap; va_start(ap, fmt);
  vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);
  // pulizia: solo ASCII stampabile
  for (size_t i = 0; buf[i]; i++) {
    unsigned char c = (unsigned char)buf[i];
    if (c < 32 || c >= 127) buf[i] = '?';
  }
  Serial.println(buf);
  if (!g_mx) return;
  if (xSemaphoreTake(g_mx, pdMS_TO_TICKS(50)) == pdTRUE) {
    char stamp[24];
    time_t now = time(NULL);
    if (now > 1700000000) {                       // ora valida: data e ora vere
      struct tm tmv; localtime_r(&now, &tmv);
      strftime(stamp, sizeof(stamp), "%d/%m %H:%M:%S ", &tmv);
    } else snprintf(stamp, sizeof(stamp), "[%lus] ", (unsigned long)(millis() / 1000));
    snprintf(g_lines[g_head], LOG_LEN, "%s%s", stamp, buf);
    g_head = (g_head + 1) % LOG_LINES;
    if (g_count < LOG_LINES) g_count++;
    xSemaphoreGive(g_mx);
  }
}

String logGet(int maxLines) {
  String r;
  if (!g_mx) return r;
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
