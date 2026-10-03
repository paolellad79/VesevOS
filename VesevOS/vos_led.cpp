// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_led.cpp
#include "vos_led.h"
#include "vos_config.h"
#include "vos_sys.h"
#include "vos_pins.h"
#include "vos_log.h"
#include <math.h>

static volatile NetState g_net = NET_BOOT;
static volatile bool g_fault = false;
static volatile bool g_reconf = true;
static uint8_t g_pin = VOS_PIN_LED_RGB;
static int g_pin2 = -1;                 // pin attuale del LED aggiuntivo (-1 = nessuno)

// Pin permessi per il LED aggiuntivo: liberi, non flash/PSRAM, non USB, non BOOT, non RGB
bool led2PinAllowed(int p) {
  if (p == 21) return true;
  if (p >= 1 && p <= 18) return true;
  if (p >= 38 && p <= 47) return true;
  return false;
}

// forma del battito: due impulsi ravvicinati, valore 0..1
static float beat(uint32_t t, uint32_t period) {
  float x = (float)(t % period) / (float)period;
  float a = expf(-powf((x - 0.10f) / 0.05f, 2));
  float b = expf(-powf((x - 0.26f) / 0.06f, 2)) * 0.7f;
  float v = a + b;
  return v > 1.0f ? 1.0f : v;
}

static void led2Setup() {
  int want = (cfg.led2Mode != 0 && led2PinAllowed(cfg.led2Pin)) ? cfg.led2Pin : -1;
  if (want == g_pin2) return;
  if (g_pin2 >= 0) { pinMode(g_pin2, INPUT); pinRelease(g_pin2); }
  g_pin2 = -1;
  if (want >= 0) {
    if (pinIsUsed(want)) { vlog("LED2: pin %d gia usato da altri", want); return; }
    pinMode(want, OUTPUT);   // semplice acceso/spento (come nello sketch che funzionava), niente PWM
    pinClaim(want, "LED2", "LED aggiuntivo (rosso)", false);
    g_pin2 = want;
  }
}

static void led2Update(uint32_t now) {
  if (g_pin2 < 0) return;
  bool on = false;
  if (cfg.led2Mode == 1) on = true;
  else if (cfg.led2Mode == 2) {
    uint32_t period = 1600 - 11 * constrain(sysCpuPercent(), 0, 100);
    on = beat(now, period) > 0.5f;   // battito: due impulsi, LED solo acceso/spento
  }
  digitalWrite(g_pin2, (on != cfg.led2Invert) ? HIGH : LOW);
}

void ledSetNetState(NetState s) { g_net = s; }
void ledSetFault(bool on) { g_fault = on; }
void ledApplyConfig() { g_reconf = true; }

static void put(uint8_t r, uint8_t g, uint8_t b, uint8_t bright) {
  uint16_t R = (uint16_t)r * bright / 255;
  uint16_t G = (uint16_t)g * bright / 255;
  uint16_t B = (uint16_t)b * bright / 255;
  rgbLedWrite(g_pin, R, G, B);
}

static void stateColor(NetState s, uint8_t& r, uint8_t& g, uint8_t& b) {
  switch (s) {
    case NET_BOOT:       r = 255; g = 120; b = 0;   break;  // arancio
    case NET_AP:         r = 0;   g = 0;   b = 255; break;  // blu
    case NET_CLIENT_TRY: r = 255; g = 255; b = 0;   break;  // giallo
    case NET_CLIENT_OK:  r = 0;   g = 255; b = 0;   break;  // verde
  }
}

static void ledTask(void*) {
  for (;;) {
    if (g_reconf) {
      g_reconf = false;
      if (g_pin != cfg.ledPin) { pinRelease(g_pin); g_pin = cfg.ledPin; }
      pinClaim(g_pin, "LED", "LED RGB WS2812", g_pin == VOS_PIN_LED_RGB);
      led2Setup();
    }
    uint32_t now = millis();
    led2Update(now);
    if (g_fault) {
      put(255, 0, 0, ((now / 150) & 1) ? cfg.ledBrightness : 0);
      vTaskDelay(pdMS_TO_TICKS(30)); continue;
    }
    uint8_t r = 0, g = 0, b = 0;
    switch (cfg.ledMode) {
      case LED_OFF: put(0, 0, 0, 0); vTaskDelay(pdMS_TO_TICKS(20)); continue;
      case LED_FIXED:
        put((cfg.ledColor >> 16) & 255, (cfg.ledColor >> 8) & 255, cfg.ledColor & 255, cfg.ledBrightness);
        vTaskDelay(pdMS_TO_TICKS(20)); continue;
      case LED_HEARTBEAT:
        r = (cfg.ledColor >> 16) & 255; g = (cfg.ledColor >> 8) & 255; b = cfg.ledColor & 255;
        break;
      default:
        stateColor(g_net, r, g, b);
        break;
    }
    // battito: periodo piu corto se la CPU e piu carica
    int cpu = sysCpuPercent();
    uint32_t period = 1600 - 11 * constrain(cpu, 0, 100);
    float v = beat(now, period);
    uint8_t br = (uint8_t)(cfg.ledBrightness * (0.08f + 0.92f * v));
    put(r, g, b, br);
    vTaskDelay(pdMS_TO_TICKS(20));
  }
}

void ledInit() {
  g_pin = cfg.ledPin;
  pinClaim(g_pin, "LED", "LED RGB WS2812", g_pin == VOS_PIN_LED_RGB);
  xTaskCreatePinnedToCore(ledTask, "led", 3072, NULL, 1, NULL, 1);
}
