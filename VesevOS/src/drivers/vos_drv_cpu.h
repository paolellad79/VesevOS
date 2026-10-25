// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_drv_cpu.h
// STRATO 1 (driver): frequenza della CPU con il gestore di energia di ESP-IDF (esp_pm, scala la frequenza da solo).
// Due modi: AUTOMATICO (80 MHz a riposo, sale fino al massimo sotto carico) e FISSO (minimo = massimo).
// Le sole frequenze ammesse sono 80, 160 e 240 MHz.
#pragma once
#include <stdint.h>
#include <soc/rtc.h>

// Frequenza di questo istante in MHz (con l'automatico cambia da sola).
inline uint32_t drvCpuMhz() {
  rtc_cpu_freq_config_t conf;
  rtc_clk_cpu_freq_get_config(&conf);
  return conf.freq_mhz;
}

// true se il valore e uno dei tre ammessi.
inline bool drvCpuMhzValid(uint32_t mhz) { return mhz == 80 || mhz == 160 || mhz == 240; }

// true se il firmware e costruito con il gestore di energia (CONFIG_PM_ENABLE). Se no, i cambi qui sotto rispondono false.
bool drvCpuPmReady();

// Automatico: minimo 80 MHz, massimo maxMhz (80, 160 o 240). false = valore non ammesso o gestore non disponibile.
bool drvCpuAuto(uint32_t maxMhz);

// Fisso: minimo = massimo = mhz. false = valore non ammesso o gestore non disponibile.
bool drvCpuFixed(uint32_t mhz);

// Massimo in vigore (l'ultimo applicato; 0 se ancora nessuno).
uint32_t drvCpuMaxMhz();
