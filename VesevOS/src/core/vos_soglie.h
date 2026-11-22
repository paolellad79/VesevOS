// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_soglie.h
// STRATO 2 (core): soglie del coprocessore ULP (S3). Controlla le righe con i limiti e le passa al driver (drivers/vos_drv_ulp.h).
// Strada 1 dell'analisi `analisi-ulp-2026-11-21`, PRIMA FETTA (driver e core). NESSUN chiamante per ora: servono le "porte" tra gli strati
// (script, shell e pagina stanno piu in alto e il core non puo chiamarli). NIENTE configurazione persistente in questa fetta:
// le righe vivono solo finche la scheda e accesa.
// I LIMITI sono una PROPOSTA da misurare sulla scheda, non decisioni: periodo 10-60000 ms, soglie e isteresi 0-4095 (l'ADC a 12 bit),
// campioni 1-255, al massimo tante righe quante ne dice il driver (oggi 8). Livello: condizioni alto/basso. Analogico: sopra/sotto/fuori/dentro
// banda, con soglia1 <= soglia2 per le bande.
#pragma once
#include <stdint.h>
#include "vos_errbuf.h"
#include "../drivers/vos_drv_ulp.h"

#define SOGLIE_PERIODO_MIN_MS 10        // PROPOSTA
#define SOGLIE_PERIODO_MAX_MS 60000     // PROPOSTA
#define SOGLIE_VALORE_MAX 4095          // PROPOSTA: soglie e isteresi dell'analogico (ADC a 12 bit)
#define SOGLIE_CAMPIONI_MAX 255         // PROPOSTA

// Una riga: stessi campi del driver, piu "attiva" (una riga non attiva si salta). sorgente, condizione, azione, resistenza hanno i valori di DrvUlp*.
struct SogliaRiga {
  int sorgente;
  int piedino;
  int condizione;
  uint32_t soglia1;
  uint32_t soglia2;
  uint32_t isteresi;
  uint32_t campioni;
  uint32_t azione;
  int resistenza;
  bool attiva;
};

struct SogliaStato {
  uint32_t valore;
  bool vera;
  bool scattata;
  uint32_t cicli;
};

bool sogliaControlla(const SogliaRiga& r, ErrBuf& err);                                   // limiti di UNA riga
bool soglieApplica(const SogliaRiga* righe, int n, uint32_t periodoMs, ErrBuf& err);      // controlla tutto, poi avvia il coprocessore (le righe non attive si saltano; senza righe attive lo ferma)
bool sogliaStato(int i, SogliaStato& s);                                                  // i = posizione tra le righe ATTIVE passate a soglieApplica
void soglieFerma();
