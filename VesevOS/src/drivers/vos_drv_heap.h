// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_drv_heap.h
// STRATO 1 (driver): memoria per dati grandi che vivono a lungo. Preferisce la PSRAM (la RAM interna e scarsa); se non c'e, usa la RAM normale.
// Vale solo per dati toccati da task (mai da interrupt, ne da codice che deve girare con la cache della flash spenta).
#pragma once
#include <stdlib.h>
#include <esp_heap_caps.h>

// Blocco azzerato di n byte. nullptr se non c'e memoria. Il chiamante controlla il risultato.
inline void* drvCallocBig(size_t n) {
  void* p = heap_caps_calloc(1, n, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  return p ? p : calloc(1, n);
}

// Blocco di n byte non azzerato: PSRAM se c'e, altrimenti RAM normale. nullptr se non c'e memoria.
inline void* drvMallocBig(size_t n) {
  void* p = heap_caps_malloc(n, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  return p ? p : malloc(n);
}

// PSRAM e basta (nullptr se la scheda non ne ha o e piena): per i buffer che senza PSRAM non devono esistere.
inline void* drvPsramMalloc(size_t n) { return heap_caps_malloc(n, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT); }

// Memoria interna (la PSRAM non e contata): totale, libera, minima dall'avvio, blocco libero piu grande.
inline uint32_t drvHeapTotal()    { return heap_caps_get_total_size(MALLOC_CAP_INTERNAL); }
inline uint32_t drvHeapFree()     { return heap_caps_get_free_size(MALLOC_CAP_INTERNAL); }
inline uint32_t drvHeapMinFree()  { return heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL); }
inline uint32_t drvHeapMaxBlock() { return heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL); }
// PSRAM: 0 se la scheda non ne ha
inline uint32_t drvPsramTotal()   { return heap_caps_get_total_size(MALLOC_CAP_SPIRAM); }
inline uint32_t drvPsramFree()    { return heap_caps_get_free_size(MALLOC_CAP_SPIRAM); }

// Da ora in poi i blocchi di memoria di almeno `limit` byte (malloc, new) vanno in PSRAM e lasciano libera la RAM interna.
// Solo con la PSRAM presente (drvPsramTotal() > 0).
inline void drvExtmemEnable(size_t limit) { heap_caps_malloc_extmem_enable(limit); }
