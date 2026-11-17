// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_led_trigger.cpp
// Logica dei trigger del LED (vedi vos_led_trigger.h). Costo: ogni 20 ms si leggono al massimo 4 contatori atomici; nessuna memoria.
#include "vos_led_trigger.h"
#include "../core/vos_common.h"
#include "../core/vos_config.h"
#include "../core/vos_sys.h"
#include "../drivers/vos_drv_activity.h"
#include <string.h>
#include <math.h>

namespace {

const uint32_t PULSE_MS = 70;     // durata di un lampo
const uint32_t CYCLE_MS = 340;    // distanza minima tra l'inizio di due lampi: al massimo 3 al secondo

struct NameMode { const char* name; uint8_t mode; };
const NameMode kNames[] = {
  {"state", LED_STATE}, {"heartbeat", LED_HEARTBEAT}, {"default-on", LED_FIXED}, {"none", LED_OFF},
  {"flash", LED_FLASH}, {"wifi", LED_WIFI}, {"cpu", LED_CPU},
  {"fixed", LED_FIXED}, {"off", LED_OFF},                      // vecchi nomi, restano validi
};

// Stato dei lampi: quando e iniziato l'ultimo, di che colore, e i contatori visti l'ultima volta
struct Pulse {
  uint32_t start;
  uint32_t color;
  bool     active;
  uint8_t  mode;                  // modo per cui valgono i contatori qui sotto
  uint32_t seen[ACT_COUNT];
};
Pulse g_pulse[LEDTRIG_SLOTS] = {};     // un blocco per LED: ognuno ha i suoi contatori visti

void resetBaseline(Pulse& p, uint8_t mode) {
  for (int i = 0; i < ACT_COUNT; i++) p.seen[i] = drvActGet((DrvAct)i);
  p.mode = mode;
  p.active = false;
}

bool changed(const Pulse& p, DrvAct k) { return drvActGet(k) != p.seen[k]; }

// Guarda i contatori del modo attuale; se e cambiato qualcosa torna il colore del lampo, altrimenti -1
int32_t pickColor(const Pulse& p, uint8_t mode) {
  if (mode == LED_FLASH) {
    bool wr = changed(p, ACT_FS_WRITE) || changed(p, ACT_SD_WRITE);
    bool rd = changed(p, ACT_FS_READ) || changed(p, ACT_SD_READ);
    if (wr) return (int32_t)cfg.ledSrcColor[LEDSRC_FLASH_W];
    if (rd) return (int32_t)cfg.ledSrcColor[LEDSRC_FLASH_R];
  } else {
    if (changed(p, ACT_WIFI_TX)) return (int32_t)cfg.ledSrcColor[LEDSRC_WIFI_TX];
    if (changed(p, ACT_WIFI_RX)) return (int32_t)cfg.ledSrcColor[LEDSRC_WIFI_RX];
  }
  return -1;
}

int clampThr(int thr) {
  if (thr < 10) return 10;
  if (thr > 95) return 95;
  return thr;
}

}  // namespace

bool ledTrigPulse(int slot, uint8_t mode, uint32_t nowMs, uint32_t* color) {
  if (slot < 0 || slot >= LEDTRIG_SLOTS) return false;
  Pulse& p = g_pulse[slot];
  if (mode != LED_FLASH && mode != LED_WIFI) { p.mode = 0xFF; return false; }
  if (p.mode != mode) resetBaseline(p, mode);                 // appena scelto: non mostrare l'attivita di prima

  uint32_t age = nowMs - p.start;
  if (!p.active || age >= CYCLE_MS) {
    int32_t c = pickColor(p, mode);
    if (c < 0) { p.active = false; return false; }
    for (int i = 0; i < ACT_COUNT; i++) p.seen[i] = drvActGet((DrvAct)i);
    p.start = nowMs; p.color = (uint32_t)c; p.active = true;
    age = 0;
  }
  if (age >= PULSE_MS) return false;                          // lampo finito, si aspetta la fine del ciclo
  if (color) *color = p.color;
  return true;
}

bool ledTrigOverlay(uint32_t nowMs, uint8_t& r, uint8_t& g, uint8_t& b) {
  uint32_t c = 0;
  if (!ledTrigPulse(0, cfg.ledMode, nowMs, &c)) return false;
  r = (c >> 16) & 255; g = (c >> 8) & 255; b = c & 255;
  return true;
}

float ledTrigBeat(uint32_t t, uint32_t period) {
  float x = (float)(t % period) / (float)period;
  float a = expf(-powf((x - 0.10f) / 0.05f, 2));
  float b = expf(-powf((x - 0.26f) / 0.06f, 2)) * 0.7f;
  float v = a + b;
  return v > 1.0f ? 1.0f : v;
}

bool ledTrigCpuOver(uint8_t thr) {
  return sysCpuPercent() > clampThr(thr);
}

bool ledTrigCpuMix(uint8_t& r, uint8_t& g, uint8_t& b) {
  int load = sysCpuPercent();
  int thr = clampThr(cfg.ledCpuThr);
  if (load <= thr) return false;
  int t = (load - thr) * 256 / (100 - thr);                   // 0..256
  if (t > 256) t = 256;
  uint32_t c = cfg.ledSrcColor[LEDSRC_CPU];
  int tr = (c >> 16) & 255, tg = (c >> 8) & 255, tb = c & 255;
  r = (uint8_t)(r + (tr - r) * t / 256);
  g = (uint8_t)(g + (tg - g) * t / 256);
  b = (uint8_t)(b + (tb - b) * t / 256);
  return true;
}

const char* ledTrigName(uint8_t mode) {
  for (const NameMode& n : kNames)
    if (n.mode == mode) return n.name;                        // il primo e il nome ufficiale
  return "state";
}

bool ledTrigFromName(const char* name, uint8_t& mode) {
  for (const NameMode& n : kNames) {
    if (!strcmp(name, n.name)) { mode = n.mode; return true; }
  }
  return false;
}
