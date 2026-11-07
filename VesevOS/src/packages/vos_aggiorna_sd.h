// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_aggiorna_sd.h
// Passaggio dalla microSD per l'installazione automatica: il firmware si scarica sulla scheda mentre VesevOS lavora e si
// controlla. Solo se e tutto giusto si riavvia nel recovery, che lo scrive leggendolo dalla microSD, senza rete.
#pragma once
#include <stddef.h>
#include "../core/vos_errbuf.h"
#include "vos_aggiorna_dati.h"

enum AggSdEsito : uint8_t {
  AGG_SD_PRONTO,      // file controllato e sulla scheda: si puo dare l'ordine
  AGG_SD_ASSENTE,     // niente microSD montata: vale la strada della rete
  AGG_SD_PIENA,       // poco spazio libero: vale la strada della rete
  AGG_SD_ERRORE       // microSD o file non vanno: niente riavvio, motivo in err
};

// Copia di riserva del firmware in uso, per tornare indietro se quello nuovo non si avvia.
struct AggRiserva {
  bool pronta;                                  // false = niente riserva: si installa lo stesso
  uint32_t dim;
  char sha[65];
};

AggSdEsito aggSdPrepara(const AggDisponibile& d, ErrBuf& err);
bool aggSdRiserva(AggRiserva& r, ErrBuf& err);              // false = microSD non affidabile: niente riavvio. Poco spazio = true, senza riserva
bool aggSdOrdine(const AggDisponibile& d, const AggRiserva& r, ErrBuf& err);     // dopo AGG_SD_PRONTO
void aggSdPulisci();                                        // toglie il file rimasto (riuscita gia avvenuta o tentativo interrotto)
