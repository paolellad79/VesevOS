// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_power.cpp
#include "vos_power.h"
#include "vos_common.h"
#include "vos_config.h"
#include "vos_net.h"
#include "vos_led.h"
#include "vos_log.h"
#include "vos_i18n.h"
#include "vos_web.h"
#include <WiFi.h>
#include <esp_wifi.h>
#include <esp_sleep.h>
#include <driver/gpio.h>

#ifndef RTC_DATA_ATTR
#define RTC_DATA_ATTR
#endif
#define PW_MIN_AWAKE 10                              // mai dormire nei primi 10 minuti dall'accensione
RTC_DATA_ATTR static uint32_t r_magic = 0, r_wakes = 0, r_sleptSec = 0, r_plan = 0;
static int g_psApplied = -1;
static uint32_t g_lastCheck = 0;

void powerBegin() {
  if (r_magic == 0xA55A1234 && esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_TIMER) {
    r_wakes++; r_sleptSec += r_plan;
    vlog("ENERGIA: risveglio dal sonno (timer), dormito circa %lu s, risvegli %lu", (unsigned long)r_plan, (unsigned long)r_wakes);
  }
  r_magic = 0;
}

void powerSet(int mode, uint32_t awakeMin, uint32_t sleepMin, String& err) {
  if (mode < 0 || mode > 2) { err = tr("Modo non valido"); return; }
  if (mode == 2) {
    if (awakeMin < PW_MIN_AWAKE || awakeMin > 1440) { err = tr("Tempo da sveglia: da 10 minuti a 24 ore"); return; }
    if (sleepMin < 1 || sleepMin > 10080) { err = tr("Sonno: da 1 minuto a 7 giorni"); return; }
    cfg.pwAwake = awakeMin; cfg.pwSleep = sleepMin;
  }
  cfg.pwMode = mode;
}

void powerSleepNow(uint32_t seconds) {
  vlog(seconds ? "ENERGIA: sonno profondo per %lu s" : "ENERGIA: sonno profondo fino a RESET (%lu)", (unsigned long)seconds);
  Serial.flush();
  ledShutdown();
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  delay(100);
  esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
  if (seconds) {
    r_magic = 0xA55A1234; r_plan = seconds;
    esp_sleep_enable_timer_wakeup((uint64_t)seconds * 1000000ULL);     // il tasto BOOT NON risveglia: tenuto premuto porterebbe in modo programmazione
  }
  esp_deep_sleep_start();
}

static void applyPs() {
  int want = cfg.pwMode == 1 ? 2 : 1;                 // 2 = massimo risparmio, 1 = normale
  int st = (int)netState() * 10 + want;
  if (st == g_psApplied) return;
  g_psApplied = st;
  esp_wifi_set_ps(want == 2 ? WIFI_PS_MAX_MODEM : WIFI_PS_MIN_MODEM);
}

static uint32_t sleepInSec() {
  if (cfg.pwMode != 2 || !cfg.setupDone) return 0;
  uint32_t up = millis() / 1000, need = cfg.pwAwake * 60UL;
  return up >= need ? 0 : need - up;
}

void powerTick() {
  uint32_t now = millis();
  if (now - g_lastCheck < 2000) return;
  g_lastCheck = now;
  applyPs();
  if (cfg.pwMode != 2 || !cfg.setupDone) return;
  if (sleepInSec() > 0) return;
  if (webIdleSec() < 60) return;                       // qualcuno sta usando la pagina: si aspetta
  powerSleepNow(cfg.pwSleep * 60UL);
}

String powerJson() {
  return "{\"mode\":" + String(cfg.pwMode) + ",\"awake\":" + String(cfg.pwAwake) + ",\"sleep\":" + String(cfg.pwSleep) + ",\"in\":" + String((unsigned long)(cfg.pwMode == 2 ? (cfg.setupDone ? sleepInSec() : 0) : 0)) +
         ",\"wakes\":" + String((unsigned long)r_wakes) + ",\"slept\":" + String((unsigned long)r_sleptSec) + ",\"guide\":" + String(cfg.setupDone ? "true" : "false") + "}";
}

String powerText() {
  String t;
  if (cfg.pwMode == 0) t = String(tr("Risparmio energia: spento")) + "\n";
  else if (cfg.pwMode == 1) t = String(tr("Risparmio energia: Wi-Fi a risparmio massimo (raggiungibile, piu lenta)")) + "\n";
  else t = trf("Risparmio energia: sonno a cicli, sveglia %u min, dorme %u min", (unsigned)cfg.pwAwake, (unsigned)cfg.pwSleep) + "\n";
  t += trf("Risvegli dal sonno: %lu, tempo dormito circa %lu s", (unsigned long)r_wakes, (unsigned long)r_sleptSec) + "\n";
  return t;
}
