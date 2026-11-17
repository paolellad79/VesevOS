// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_drv_cpu.cpp (strato 1: frequenza della CPU con esp_pm, vedi vos_drv_cpu.h)
// Questo e l'UNICO punto che chiama esp_pm_configure; un mutex evita chiamate insieme da due task.
// Il sonno leggero resta spento (light_sleep_enable = false): qui si scala solo la frequenza.
// Il tick di FreeRTOS e i timer usano il SYSTIMER (CONFIG_FREERTOS_SYSTICK_USES_SYSTIMER), che non dipende dalla CPU.
#include "vos_drv_cpu.h"
#include <sdkconfig.h>
#include <mutex>
#if CONFIG_PM_ENABLE
#include <esp_pm.h>
#endif

static std::mutex s_lock;
static uint32_t s_maxMhz = 0;

// ---- frequenze valide del chip (calcolate una volta, poi in cache) ----
static const uint32_t CANDIDATE_MHZ[] = {80, 120, 160, 240};
static const uint32_t MID_TARGET_MHZ = 160;   // il gradino di mezzo e il valido piu vicino a questo
static const int MAX_LEVELS = 4;

struct CpuLevels {
  uint32_t mhz[MAX_LEVELS];
  int count;
  uint32_t mid;
};

static CpuLevels buildLevels() {
  CpuLevels L = {};
  for (uint32_t cand : CANDIDATE_MHZ) {
    rtc_cpu_freq_config_t conf;
    if (rtc_clk_cpu_freq_mhz_to_config(cand, &conf) && L.count < MAX_LEVELS) L.mhz[L.count++] = cand;
  }
  if (L.count == 0) {                       // il chip non ha risposto: ripiego sul file di scheda
    L.mhz[0] = VOS_CPU_MHZ_MIN;
    L.mhz[1] = VOS_CPU_MHZ_MID;
    L.mhz[2] = VOS_CPU_MHZ_MAX;
    L.count = 3;
  }
  L.mid = L.mhz[0];
  uint32_t best = 0xFFFFFFFFu;
  for (int i = 0; i < L.count; i++) {
    uint32_t d = L.mhz[i] > MID_TARGET_MHZ ? L.mhz[i] - MID_TARGET_MHZ : MID_TARGET_MHZ - L.mhz[i];
    if (d < best) { best = d; L.mid = L.mhz[i]; }
  }
  return L;
}

static const CpuLevels& levels() {
  static const CpuLevels L = buildLevels();   // inizializzazione protetta (thread-safe) in C++11
  return L;
}

int drvCpuLevels(uint32_t* out, int maxOut) {
  const CpuLevels& L = levels();
  for (int i = 0; i < L.count && i < maxOut; i++) out[i] = L.mhz[i];
  return L.count;
}

uint32_t drvCpuMin() { return levels().mhz[0]; }
uint32_t drvCpuMax() { const CpuLevels& L = levels(); return L.mhz[L.count - 1]; }
uint32_t drvCpuMid() { return levels().mid; }

bool drvCpuMhzValid(uint32_t mhz) {
  const CpuLevels& L = levels();
  for (int i = 0; i < L.count; i++) if (L.mhz[i] == mhz) return true;
  return false;
}

bool drvCpuPmReady() {
#if CONFIG_PM_ENABLE
  return true;
#else
  return false;     // costruzione di prova senza gestore di energia: nessun cambio di frequenza possibile
#endif
}

// Applica minimo e massimo (da chiamare con il mutex preso).
static bool apply(uint32_t minMhz, uint32_t maxMhz) {
#if CONFIG_PM_ENABLE
  esp_pm_config_t conf = {};
  conf.max_freq_mhz = (int)maxMhz;
  conf.min_freq_mhz = (int)minMhz;
  conf.light_sleep_enable = false;
  if (esp_pm_configure(&conf) != ESP_OK) return false;
  s_maxMhz = maxMhz;
  return true;
#else
  (void)minMhz; (void)maxMhz;
  return false;
#endif
}

bool drvCpuAuto(uint32_t maxMhz) {
  if (!drvCpuMhzValid(maxMhz)) return false;
  std::lock_guard<std::mutex> g(s_lock);
  return apply(drvCpuMin(), maxMhz);
}

bool drvCpuFixed(uint32_t mhz) {
  if (!drvCpuMhzValid(mhz)) return false;
  std::lock_guard<std::mutex> g(s_lock);
  return apply(mhz, mhz);
}

uint32_t drvCpuMaxMhz() {
  std::lock_guard<std::mutex> g(s_lock);
  return s_maxMhz;
}
