// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_aggiorna.h
// Gestione degli aggiornamenti: legge dal sito del progetto cosa e stato pubblicato, lo confronta con la scheda e lo segnala.
// Spenta di fabbrica. Il controllo gira in un task proprio, uno per volta, mai nel task del server web.
// "Installa da solo" riguarda solo il firmware ufficiale con versione piu alta, mai il recovery: app e driver danno solo la notifica.
// L'installazione vera la fa il recovery con l'ordine di core/vos_updorder.h.
#pragma once
#include <stddef.h>
#include "../core/vos_errbuf.h"
#include "../core/vos_heapbuf.h"

void aggiornaInit();                               // avvio: rilegge la data dell'ultimo controllo
void aggiornaTick();                               // dal ciclo principale: fa partire il controllo programmato
bool aggiornaVerifica(ErrBuf& err);                // il tasto "Verifica ora": avvia il controllo e torna subito
// Il tasto "Installa": ordine al recovery per la versione trovata (messa in versione); il riavvio lo fa il chiamante.
bool aggiornaInstalla(char* versione, size_t cap, ErrBuf& err);
bool aggiornaStatoJson(HeapBuf& j);                // GET /api/aggiorna
bool aggiornaPacchettiJson(HeapBuf& j);            // GET /api/aggiorna/pacchetti
bool aggiornaNotaJson(HeapBuf& j);                 // campo "agg" di /api/status: {"k":tipo,"m":modo,"v":testo}; tipo 0 = niente da segnalare
void aggiornaNotaVista();                          // l'amministratore ha aperto la pagina: esito segnalato una volta sola
