// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_drv_activity.h
// STRATO 1 (driver): contatori di attivita per il LED (e per chi vuole sapere "e successo qualcosa?").
// Solo numeri che crescono: nessun nome di file, nessun indirizzo, nessun contenuto. Gli incrementi sono atomici e leggeri
// (una istruzione, nessun blocco, nessuna memoria): si possono chiamare da qualunque task e dal codice di rete.
// Chi legge (il task del LED) confronta il valore di adesso con quello di prima: se e diverso, c'e stata attivita.
// Tutto in questo file (variabile inline): niente .cpp, quindi vale anche per le prove sul computer.
#pragma once
#include <stdint.h>

enum DrvAct : uint8_t {
  ACT_FS_READ = 0,     // letture dalla flash interna (LittleFS): una per chiamata
  ACT_FS_WRITE,        // scritture sulla flash interna
  ACT_SD_READ,         // letture dalla microSD
  ACT_SD_WRITE,        // scritture sulla microSD
  ACT_WIFI_RX,         // byte ricevuti dal Wi-Fi (STA e hotspot)
  ACT_WIFI_TX,         // byte inviati dal Wi-Fi
  ACT_COUNT
};

inline uint32_t g_drvAct[ACT_COUNT];   // si riavvolgono: chi legge usa la differenza

inline void drvActAdd(DrvAct k, uint32_t n = 1) { __atomic_fetch_add(&g_drvAct[k], n, __ATOMIC_RELAXED); }
inline uint32_t drvActGet(DrvAct k) { return __atomic_load_n(&g_drvAct[k], __ATOMIC_RELAXED); }
