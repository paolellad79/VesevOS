// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_drv_cpu.h
// STRATO 1 (driver): frequenza della CPU con il gestore di energia di ESP-IDF (esp_pm, scala la frequenza da solo).
// Due modi: AUTOMATICO (80 MHz a riposo, sale fino al massimo sotto carico) e FISSO (minimo = massimo).
// Le frequenze ammesse le dice il CHIP (rtc_clk_cpu_freq_mhz_to_config di soc/rtc.h, la stessa funzione con cui esp_pm_configure
// controlla i valori), provando 80, 120, 160, 240. Calcolo una volta sola. Se nessuna risulta valida si ripiega sui macro del file di
// scheda (VOS_CPU_MHZ_MIN/MID/MAX).
#pragma once
#include <stdint.h>
#include <soc/rtc.h>
#include "../boards/vos_board.h"

// Frequenza di questo istante in MHz (con l'automatico cambia da sola).
inline uint32_t drvCpuMhz() {
  rtc_cpu_freq_config_t conf;
  rtc_clk_cpu_freq_get_config(&conf);
  return conf.freq_mhz;
}

// Frequenze valide per il chip, in ordine crescente. Copia al massimo maxOut valori e restituisce quanti ce ne sono.
int drvCpuLevels(uint32_t* out, int maxOut);

// Gradino piu basso, piu alto e di mezzo (il valido piu vicino a 160) tra quelli validi.
uint32_t drvCpuMin();
uint32_t drvCpuMax();
uint32_t drvCpuMid();

// true se il valore e una delle frequenze valide del chip.
bool drvCpuMhzValid(uint32_t mhz);

// true se il firmware e costruito con il gestore di energia (CONFIG_PM_ENABLE). Se no, i cambi qui sotto rispondono false.
bool drvCpuPmReady();

// Automatico: minimo 80 MHz, massimo maxMhz (80, 160 o 240). false = valore non ammesso o gestore non disponibile.
bool drvCpuAuto(uint32_t maxMhz);

// Fisso: minimo = massimo = mhz. false = valore non ammesso o gestore non disponibile.
bool drvCpuFixed(uint32_t mhz);

// Massimo in vigore (l'ultimo applicato; 0 se ancora nessuno).
uint32_t drvCpuMaxMhz();
