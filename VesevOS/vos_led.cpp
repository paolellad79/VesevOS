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

// forma del battito: due impulsi ravvicinati, valore 0..1
static float beat(uint32_t t, uint32_t period) {
  float x = (float)(t % period) / (float)period;
  float a = expf(-powf((x - 0.10f) / 0.05f, 2));
  float b = expf(-powf((x - 0.26f) / 0.06f, 2)) * 0.7f;
  float v = a + b;
  return v > 1.0f ? 1.0f : v;
}

void ledSetNetState(NetState s) { g_net = s; }
void ledSetFault(bool on) { g_fault = on; }
static volatile uint32_t g_identUntil = 0;   // 0 = spento, altrimenti millis() di fine
void ledIdentify(uint32_t ms) { g_identUntil = millis() + ms; if (!g_identUntil) g_identUntil = 1; }
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
    case NET_AIR:        r = 170; g = 0;   b = 255; break;  // viola (modalita aereo)
  }
}

static void ledTask(void*) {
  for (;;) {
    if (g_reconf) {
      g_reconf = false;
      if (g_pin != cfg.ledPin) { pinRelease(g_pin); g_pin = cfg.ledPin; }
      pinClaim(g_pin, "LED", "LED RGB WS2812", g_pin == VOS_PIN_LED_RGB);
    }
    uint32_t now = millis();
    if (g_identUntil) {
      if ((int32_t)(g_identUntil - now) > 0) {
        // arcobaleno: tinta che gira (0..5 settori da 60 gradi)
        uint32_t h = (now / 4) % 360, f = h % 60, q = 255 * f / 60;
        uint8_t r, g, b;
        switch (h / 60) {
          case 0: r = 255; g = q; b = 0; break;
          case 1: r = 255 - q; g = 255; b = 0; break;
          case 2: r = 0; g = 255; b = q; break;
          case 3: r = 0; g = 255 - q; b = 255; break;
          case 4: r = q; g = 0; b = 255; break;
          default: r = 255; g = 0; b = 255 - q; break;
        }
        put(r, g, b, cfg.ledBrightness < 80 ? 80 : cfg.ledBrightness);
        vTaskDelay(pdMS_TO_TICKS(20)); continue;
      }
      g_identUntil = 0;
    }
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

static void ledStart() { g_reconf = true; xTaskCreatePinnedToCore(ledTask, "led", 3072, NULL, 1, NULL, 1); }

void ledInit() {
  g_pin = cfg.ledPin;
  pinClaim(g_pin, "LED", "LED RGB WS2812", g_pin == VOS_PIN_LED_RGB);
  sysTaskRegister("led", ledStart);
  ledStart();
}

void ledShutdown() {
  TaskHandle_t h = xTaskGetHandle("led");
  if (h) vTaskDelete(h);
  rgbLedWrite(g_pin, 0, 0, 0);
}
