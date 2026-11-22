// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_rec_cfg.h
// Impostazioni e misure comuni del registratore di dati (campioni in vos_rec.cpp, azioni in vos_rec_ev.cpp).
#pragma once
#include <stdint.h>

#define REC_RING      120          // punti tenuti in RAM per il grafico
#define REC_SLOTS     6            // 4 periferiche + temperatura + RAM
#define REC_DEV_MAX   4
#define REC_SLOT_T    4
#define REC_SLOT_R    5
#define REC_JSON_MAX  16384        // tetto del JSON (120 punti x 6 valori + ultime azioni stanno in circa 11 KB)
#define REC_ND        INT32_MIN    // valore non disponibile (lettura fallita o fonte non letta in quel momento)

struct RecSettings {
  bool     on;
  uint16_t sec;                    // secondi tra due campioni (predefinito per le fonti che non hanno il proprio)
  uint8_t  where;                  // 0 flash, 1 microSD
  uint16_t maxKb;                  // spazio massimo dei file del registratore, in KB
  char     nome[25];               // nome dei file (a-z A-Z 0-9 _ -, 1-24 caratteri)
  uint8_t  limite;                 // 0 = maxKb vale per tutti i file della cartella insieme, 1 = per ogni singolo file
  bool     t, r;                   // temperatura del chip, RAM libera
  char     dev[REC_DEV_MAX][16];   // nomi di periferica ("" = nessuna)
  uint16_t secDev[REC_DEV_MAX];    // secondi tra due letture di ogni periferica (0 = usa sec)
  uint16_t secT, secR;             // lo stesso per temperatura e RAM
  uint16_t azioni;                 // categorie di azioni tracciate (un bit per categoria, core/vos_azioni.h)
  uint16_t evMax;                  // righe di azioni al giorno al massimo (0 = nessun limite)
};
