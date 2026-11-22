// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - ulp/vos_ulp_tabella.h
// Tabella di soglie condivisa tra la CPU principale e il coprocessore ULP-RISC-V dell'S3 (strada A dell'analisi `analisi-ulp-2026-11-21`):
// la CPU scrive le righe, il coprocessore le legge e scrive lo stato. Il coprocessore non interpreta niente: scorre la tabella.
// Questo file e usato da due programmi diversi (la CPU e il coprocessore): niente C++, solo tipi a 32 bit. Sta fuori da src/ perche il
// programma del coprocessore non si compila con il resto (vedi idf/main/CMakeLists.txt, ulp_embed_binary).
#pragma once
#include <stdint.h>

#define ULP_RIGHE_MAX 8                // PROPOSTA da misurare sulla memoria RTC rimasta (S3: 8 KB in tutto, programma e dati insieme)

enum { ULP_TIPO_NESSUNO = 0, ULP_TIPO_LIVELLO = 1, ULP_TIPO_ANALOGICO = 2 };
enum { ULP_COND_ALTO = 1, ULP_COND_BASSO = 2, ULP_COND_SOPRA = 3, ULP_COND_SOTTO = 4, ULP_COND_FUORI = 5, ULP_COND_DENTRO = 6 };
enum { ULP_AZ_SVEGLIA = 1, ULP_AZ_REGISTRA = 2 };       // bit; REGISTRA non e ancora eseguita dal programma (solo SVEGLIA)

// Una riga: scritta dalla CPU. canale: per il livello e il numero del piedino, per l'analogico il canale dell'ADC1 (lo ricava il driver dal piedino).
typedef struct {
  uint32_t tipo;
  uint32_t canale;
  uint32_t condizione;
  uint32_t soglia1;
  uint32_t soglia2;
  uint32_t isteresi;
  uint32_t campioni;                   // letture di fila prima di dichiarare l'evento (antirimbalzo); 0 vale 1
  uint32_t azione;
} vos_ulp_riga_t;

// Lo stato di una riga: scritto dal coprocessore, letto dalla CPU.
typedef struct {
  uint32_t valore;                     // ultimo valore letto
  uint32_t vera;                       // la condizione e vera (dopo i campioni di fila)
  uint32_t conta;                      // letture di fila con la condizione vera
  uint32_t scattata;                   // l'evento e gia stato segnalato (si azzera quando la condizione smette)
} vos_ulp_stato_t;

typedef struct {
  vos_ulp_riga_t righe[ULP_RIGHE_MAX];
  vos_ulp_stato_t stato[ULP_RIGHE_MAX];
  uint32_t cicli;                      // quante volte il coprocessore ha girato
} vos_ulp_tabella_t;
