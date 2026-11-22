// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_rec_ev.h
// Azioni sul dispositivo nel registratore: un ascoltatore sul bus degli eventi ("act.") le copia in una coda di 16 voci in RAM;
// il task del registratore la svuota e scrive eventi.csv a blocchi: epoca,categoria,nome,valore,origine. Nessun nome utente.
#pragma once
#include "vos_rec_cfg.h"
#include "../core/vos_heapbuf.h"
#include "../core/vos_strbuf.h"

void recEvBegin();                                          // crea i blocchi e si iscrive al bus (una volta)
void recEvTick(const RecSettings& s);                       // un giro al secondo (dal task): svuota la coda, scrive a blocchi, registra l'avvio
void recEvFlush(const RecSettings& s, uint32_t maxMs);      // svuota la coda e scrive subito tutto cio che e in attesa (al massimo maxMs di attesa del blocco)
void recEvReset();                                          // nuove impostazioni: il blocco in attesa e il conteggio del giorno ripartono
bool recEvJson(HeapBuf& j);                                 // aggiunge ,"ev":{...} (conteggi e ultime 20 azioni)
void recEvText(StrBuf& b);                                  // per la shell
