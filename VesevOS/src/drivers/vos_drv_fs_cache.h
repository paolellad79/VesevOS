// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_drv_fs_cache.h
// STRATO 1 (driver): quando rinfrescare lo "spazio usato" della memoria interna. Solo la regola, senza LittleFS ne FreeRTOS (si prova sul computer).
// Perche: LittleFS calcola lo spazio usato scorrendo tutto il file system. Con tante voci puo durare piu di 5 s e far scattare il Task watchdog
// (era la causa del ciclo di riavvii con una cartella piena). Quindi chi chiede lo spazio riceve l'ultimo valore ricordato, e il calcolo vero
// lo fa un solo punto (il controllo "monitor"), di rado e mai nei primi secondi dopo l'avvio.
#pragma once
#include <stddef.h>
#include <stdint.h>

#define FS_USED_FIRST_S 20      // mai prima: Wi-Fi, NTP e HTTPS si stanno assestando
#define FS_USED_DIRTY_S 60      // dopo una scrittura, non prima di 60 s dall'ultimo calcolo
#define FS_USED_EVERY_S 600     // comunque ogni 10 minuti

struct FsUsedCache {
  size_t   used = 0;            // ultimo valore (0 finche non e mai stato calcolato)
  bool     known = false;
  bool     dirty = false;       // qualcosa e stato scritto o cancellato dopo l'ultimo calcolo
  uint32_t lastSec = 0;         // secondi dall'avvio dell'ultimo calcolo

  void touch() { dirty = true; }
  // nowSec = secondi dall'avvio. La differenza e senza segno: regge il giro a 32 bit.
  bool due(uint32_t nowSec) const {
    if (nowSec < FS_USED_FIRST_S) return false;
    if (!known) return true;
    uint32_t age = nowSec - lastSec;
    return age >= FS_USED_EVERY_S || (dirty && age >= FS_USED_DIRTY_S);
  }
  void store(size_t u, uint32_t nowSec) { used = u; known = true; dirty = false; lastSec = nowSec; }
};
