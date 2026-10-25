// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_power.cpp
#include "vos_power.h"
#include "../core/vos_common.h"
#include "../core/vos_config.h"
#include "../net/vos_net.h"
#include "../devices/vos_led.h"
#include "../core/vos_log.h"
#include "../core/vos_i18n.h"
#include "../core/vos_strbuf.h"
#include "../core/vos_hooks.h"
#include "../drivers/vos_drv_wifi.h"
#include <esp_sleep.h>
#include <driver/gpio.h>
#include "../drivers/vos_drv_time.h"
#include "../drivers/vos_drv_console.h"

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

void powerSet(int mode, uint32_t awakeMin, uint32_t sleepMin, ErrBuf& err) {
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
  drvConFlush();
  ledShutdown();
  drvWifiShutdown();
  drvDelayMs(100);
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
  drvWifiPowerSave(want == 2);
}

static uint32_t sleepInSec() {
  if (cfg.pwMode != 2 || !cfg.setupDone) return 0;
  uint32_t up = drvMillis() / 1000, need = cfg.pwAwake * 60UL;
  return up >= need ? 0 : need - up;
}

void powerTick() {
  uint32_t now = drvMillis();
  if (now - g_lastCheck < 2000) return;
  g_lastCheck = now;
  applyPs();
  if (cfg.pwMode != 2 || !cfg.setupDone) return;
  if (sleepInSec() > 0) return;
  if (hookWebIdleSec() < 60) return;                       // qualcuno sta usando la pagina: si aspetta
  powerSleepNow(cfg.pwSleep * 60UL);
}

bool powerJson(HeapBuf& j) {
  unsigned long inSec = (cfg.pwMode == 2 && cfg.setupDone) ? (unsigned long)sleepInSec() : 0;
  return j.addf("{\"mode\":%d,\"awake\":%d,\"sleep\":%d,\"in\":%lu,\"wakes\":%lu,\"slept\":%lu,\"guide\":%s}",
                (int)cfg.pwMode, (int)cfg.pwAwake, (int)cfg.pwSleep, inSec, (unsigned long)r_wakes, (unsigned long)r_sleptSec,
                cfg.setupDone ? "true" : "false");
}

// due righe tradotte (al massimo ~250 byte): 512 bastano
std::string powerText() {
  StrText t(512);
  StrBuf& s = t.buf();
  if (cfg.pwMode == 0) s.add(tr("Risparmio energia: spento"));
  else if (cfg.pwMode == 1) s.add(tr("Risparmio energia: Wi-Fi a risparmio massimo (raggiungibile, piu lenta)"));
  else s.addf(tr("Risparmio energia: sonno a cicli, sveglia %u min, dorme %u min"), (unsigned)cfg.pwAwake, (unsigned)cfg.pwSleep);
  s.add('\n').addf(tr("Risvegli dal sonno: %lu, tempo dormito circa %lu s"), (unsigned long)r_wakes, (unsigned long)r_sleptSec).add('\n');
  return t.take();
}
