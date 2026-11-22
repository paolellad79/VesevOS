// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_drv_ulp.h
// STRATO 1 (driver): coprocessore ULP-RISC-V dell'S3 (strada 1 dell'analisi `analisi-ulp-2026-11-21`). La CPU scrive una TABELLA di soglie,
// un programma FISSO nostro del coprocessore (VesevOS/ulp/vos_ulp_soglie.c) la scorre e, se una riga con azione SVEGLIA scatta, sveglia la CPU.
// Solo con VOS_WITH_ULP_SOGLIE (S3); altrove le funzioni rispondono "non disponibile". Il controllo dei limiti sta nel core (vos_soglie.h):
// qui si validano solo le cose che sa il chip (piedino RTC, ADC1) con le funzioni di ESP-IDF, mai con numeri di pin scritti a mano.
// Il valore di "condizione" e "azione" e lo stesso della tabella condivisa (ulp/vos_ulp_tabella.h): non cambiarli da una parte sola.
#pragma once
#include <stdint.h>

#define DRV_ULP_RIGHE_MAX 8     // righe della tabella (PROPOSTA, da misurare sulla memoria RTC rimasta); uguale a ULP_RIGHE_MAX di ulp/vos_ulp_tabella.h (il .cpp lo controlla)

enum DrvUlpSorgente { DRV_ULP_LIVELLO = 1, DRV_ULP_ANALOGICO = 2 };
enum DrvUlpCondizione { DRV_ULP_ALTO = 1, DRV_ULP_BASSO = 2, DRV_ULP_SOPRA = 3, DRV_ULP_SOTTO = 4, DRV_ULP_FUORI = 5, DRV_ULP_DENTRO = 6 };
enum DrvUlpAzione { DRV_ULP_SVEGLIA = 1, DRV_ULP_REGISTRA = 2 };       // bit; REGISTRA non e ancora eseguita dal programma del coprocessore
enum DrvUlpResistenza { DRV_ULP_RES_NESSUNA = 0, DRV_ULP_RES_SU = 1, DRV_ULP_RES_GIU = 2 };

// Codici di errore di drvUlpAvvia (0 = ok). I testi li scrive chi chiama, con tr().
enum DrvUlpErr {
  DRV_ULP_OK = 0,
  DRV_ULP_E_NON_DISPONIBILE = 1,   // la scheda o la build non hanno il coprocessore
  DRV_ULP_E_PIEDINO = 2,           // il piedino non e un piedino RTC (il coprocessore non lo legge)
  DRV_ULP_E_NON_ADC1 = 3,          // l'analogico deve stare su un piedino dell'ADC1
  DRV_ULP_E_DUE_CANALI = 4,        // per ora un solo canale analogico
  DRV_ULP_E_TROPPE_RIGHE = 5,
  DRV_ULP_E_CARICAMENTO = 6,       // il programma non e entrato nella memoria del coprocessore
  DRV_ULP_E_AVVIO = 7,
  DRV_ULP_E_ADC = 8,               // l'ADC non si e configurato per il coprocessore
  DRV_ULP_E_RIGA = 9               // riga non valida (sorgente, condizione o puntatore)
};

struct DrvUlpRiga {
  int sorgente;                    // DrvUlpSorgente
  int piedino;                     // numero GPIO; per l'analogico il driver ricava il canale dell'ADC1
  int condizione;                  // DrvUlpCondizione
  uint32_t soglia1;
  uint32_t soglia2;
  uint32_t isteresi;
  uint32_t campioni;               // letture di fila prima di dichiarare l'evento (0 vale 1)
  uint32_t azione;                 // bit di DrvUlpAzione
  int resistenza;                  // DrvUlpResistenza (solo per il livello)
};

struct DrvUlpStato {
  uint32_t valore;                 // ultimo valore letto dal coprocessore
  bool vera;                       // la condizione e vera (dopo i campioni di fila)
  uint32_t conta;
  bool scattata;                   // l'evento e gia stato segnalato
  uint32_t cicli;                  // quante volte il coprocessore ha girato (uguale per tutte le righe)
};

bool drvUlpDisponibile();
int  drvUlpRigheMax();                                                       // 0 se non disponibile
int  drvUlpAvvia(const DrvUlpRiga* righe, int n, uint32_t periodoUs);        // 0 = ok, altrimenti DrvUlpErr; se era gia attivo lo ferma prima
bool drvUlpStato(int i, DrvUlpStato& s);                                     // false se non attivo o i fuori misura
bool drvUlpAttivo();
void drvUlpPreparaSonno();                                                   // prima di dormire: dominio RTC acceso e risveglio dall'ULP armato
void drvUlpFerma();
