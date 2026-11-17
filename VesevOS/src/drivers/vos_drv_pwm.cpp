// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_drv_pwm.cpp
// PWM con LEDC (vedi vos_drv_pwm.h): elenco di canali e timer. L'inversione per i LED a livello basso e fatta nel calcolo
// del duty, cosi il livello a riposo e quello dello spento anche prima che il PWM giri.
#include "vos_drv_pwm.h"
#include <driver/ledc.h>
#include <esp_private/esp_clk.h>
#include <soc/soc_caps.h>
#include <freertos/FreeRTOS.h>

#define PWM_MODE      LEDC_LOW_SPEED_MODE
#define PWM_BITS_MAX  14                 // percento e per mille bastano molto prima; evita divisori fuori scala
#define PWM_BITS_MIN  6                  // sotto i 64 passi la regolazione e troppo grossolana: frequenza rifiutata

struct Timer { uint32_t hz; uint8_t bits; uint8_t users; };
struct Canale { bool used; bool low; uint8_t timer; uint8_t bits; uint32_t hz; };

static Timer g_t[SOC_LEDC_TIMER_NUM];
static Canale g_c[SOC_LEDC_CHANNEL_NUM];
static portMUX_TYPE g_mux = portMUX_INITIALIZER_UNLOCKED;     // protegge solo le due tabelle, mai le chiamate al controllore

// Risoluzione che il chip sa fare a questa frequenza (0 = nessuna)
static uint8_t bitsFor(uint32_t hz, uint8_t wanted) {
  if (wanted) return wanted;
  uint32_t b = ledc_find_suitable_duty_resolution((uint32_t)esp_clk_xtal_freq(), hz);
  if (b > PWM_BITS_MAX) b = PWM_BITS_MAX;
  return b >= PWM_BITS_MIN ? (uint8_t)b : 0;
}

static uint32_t full(uint8_t bits) { return 1UL << bits; }

static uint32_t dutyOf(const Canale& c, uint32_t parte, uint32_t su) {
  uint32_t f = full(c.bits);
  uint32_t d = parte >= su ? f : (uint32_t)(((uint64_t)parte * f + su / 2) / su);
  return c.low ? f - d : d;
}

// Prenota canale e timer nelle tabelle. Ritorna il canale, o un codice di errore. tNew = il timer va configurato.
static int reserve(uint32_t hz, uint8_t bits, bool low, bool& tNew) {
  int ch = -1, tm = -1;
  tNew = false;
  portENTER_CRITICAL(&g_mux);
  for (int i = 0; i < SOC_LEDC_CHANNEL_NUM && ch < 0; i++) if (!g_c[i].used) ch = i;
  for (int i = 0; i < SOC_LEDC_TIMER_NUM && tm < 0; i++) if (g_t[i].users && g_t[i].hz == hz && g_t[i].bits == bits) tm = i;
  if (ch >= 0 && tm < 0) {
    for (int i = 0; i < SOC_LEDC_TIMER_NUM && tm < 0; i++) if (!g_t[i].users) { tm = i; tNew = true; }
  }
  if (ch >= 0 && tm >= 0) {
    g_c[ch] = {true, low, (uint8_t)tm, bits, hz};
    if (tNew) { g_t[tm].hz = hz; g_t[tm].bits = bits; }
    g_t[tm].users++;
  }
  portEXIT_CRITICAL(&g_mux);
  return ch < 0 ? DRV_PWM_E_CANALI : tm < 0 ? DRV_PWM_E_TIMER : ch;
}

void drvPwmClose(int h) {
  if (h < 0 || h >= SOC_LEDC_CHANNEL_NUM || !g_c[h].used) return;
  ledc_stop(PWM_MODE, (ledc_channel_t)h, g_c[h].low ? 1 : 0);
  portENTER_CRITICAL(&g_mux);
  uint8_t t = g_c[h].timer;
  if (g_t[t].users) g_t[t].users--;
  g_c[h] = Canale{};
  portEXIT_CRITICAL(&g_mux);
}

int drvPwmOpen(int gpio, uint32_t hz, uint8_t bits, bool activeLow) {
  if (hz < DRV_PWM_HZ_MIN || hz > DRV_PWM_HZ_MAX) return DRV_PWM_E_HZ;
  bits = bitsFor(hz, bits);
  if (!bits) return DRV_PWM_E_HZ;
  bool tNew;
  int h = reserve(hz, bits, activeLow, tNew);
  if (h < 0) return h;
  uint8_t t = g_c[h].timer;
  if (tNew) {
    ledc_timer_config_t tc = {};
    tc.speed_mode = PWM_MODE;
    tc.duty_resolution = (ledc_timer_bit_t)bits;
    tc.timer_num = (ledc_timer_t)t;
    tc.freq_hz = hz;
    tc.clk_cfg = LEDC_USE_XTAL_CLK;
    if (ledc_timer_config(&tc) != ESP_OK) { drvPwmClose(h); return DRV_PWM_E_HZ; }
  }
  ledc_channel_config_t cc = {};
  cc.gpio_num = gpio;
  cc.speed_mode = PWM_MODE;
  cc.channel = (ledc_channel_t)h;
  cc.timer_sel = (ledc_timer_t)t;
  cc.duty = dutyOf(g_c[h], 0, 1);          // parte spento
  cc.hpoint = 0;
  if (ledc_channel_config(&cc) != ESP_OK) { drvPwmClose(h); return DRV_PWM_E_HW; }
  return h;
}

void drvPwmSet(int h, uint32_t parte, uint32_t su) {
  if (h < 0 || h >= SOC_LEDC_CHANNEL_NUM || !g_c[h].used || !su) return;
  ledc_set_duty(PWM_MODE, (ledc_channel_t)h, dutyOf(g_c[h], parte, su));
  ledc_update_duty(PWM_MODE, (ledc_channel_t)h);
}

int drvPwmTotal() { return SOC_LEDC_CHANNEL_NUM; }

int drvPwmFree() {
  int n = 0;
  for (int i = 0; i < SOC_LEDC_CHANNEL_NUM; i++) if (!g_c[i].used) n++;
  return n;
}
