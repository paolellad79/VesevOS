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
  return apply(80, maxMhz);
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
