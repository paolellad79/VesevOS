// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_drv_heap.cpp
// Mappa della RAM: ogni voce viene da una misura (heap di IDF, simboli del linker) o da una sottrazione dichiarata in vos_drv_heap.h.
#include "vos_drv_heap.h"
#include "vos_drv_sys.h"
#include "../boards/vos_board.h"

// Simboli del linker di ESP-IDF. "weak": se in questa versione mancano valgono nullptr e la voce resta "non misurata" (il link non si rompe).
extern "C" {
extern char _iram_start __attribute__((weak));
extern char _iram_end __attribute__((weak));
extern char _data_start __attribute__((weak));
extern char _data_end __attribute__((weak));
extern char _bss_start __attribute__((weak));
extern char _bss_end __attribute__((weak));
}

// Byte fra due simboli; -1 se uno manca o l'ordine e sbagliato.
static int32_t spanBytes(const char* a, const char* b) {
  if (!a || !b || b < a) return -1;
  return (int32_t)(b - a);
}

void drvMemMap(DrvMemMap& m) {
  m.sramNominal = (uint32_t)VOS_SRAM_KB * 1024UL;
  m.heapTotal = drvHeapTotal();
  m.heapFree = drvHeapFree();
  m.heapUsed = m.heapTotal > m.heapFree ? m.heapTotal - m.heapFree : 0;
  m.heapMin = drvHeapMinFree();
  m.heapMaxBlock = drvHeapMaxBlock();
  m.psramTotal = drvPsramTotal();
  m.psramFree = drvPsramFree();
  m.flashSize = drvFlashSize();

  m.fixedTotal = m.sramNominal > m.heapTotal ? m.sramNominal - m.heapTotal : 0;
  m.fixedIram = spanBytes(&_iram_start, &_iram_end);
  m.fixedData = spanBytes(&_data_start, &_data_end);
  m.fixedBss  = spanBytes(&_bss_start, &_bss_end);
  // Il resto ha senso solo se le tre voci sono tutte misurate e la somma non supera la parte fissa.
  m.fixedRest = -1;
  if (m.fixedIram >= 0 && m.fixedData >= 0 && m.fixedBss >= 0) {
    int64_t rest = (int64_t)m.fixedTotal - m.fixedIram - m.fixedData - m.fixedBss;
    if (rest >= 0) m.fixedRest = (int32_t)rest;
  }
}
