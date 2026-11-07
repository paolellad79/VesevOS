// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_pacchetto_nome.h
// Dopo un caricamento: il file con il nome lungo diventa il nome corto dichiarato al suo interno e prende il posto di quello installato.
#pragma once
#include <string_view>
#include "../core/vos_errbuf.h"
#include "../core/vos_pacchetto.h"

// Cio che il file dichiara di se stesso; i campi non dichiarati restano vuoti.
void pacchettoLeggiDati(const char* percorso, PacDati& d);

// true = percorso e un pacchetto gia installato in /bin, /sbin o /devs: il nuovo va scritto a parte, per controllarlo prima di sostituire.
bool pacchettoDaProteggere(const char* percorso);

// caricato = dove sta il file ora; nomeFile = il percorso con il nome scelto da chi carica (uguale a caricato se non c'era nulla da proteggere).
// Se il pacchetto ne sostituisce uno installato, viene prima controllato: se non e valido quello installato resta intatto.
// false = caricamento rifiutato, il file caricato e gia tolto e err dice perche. avviso = da mostrare (versione piu bassa), vuoto se non c'e.
bool pacchettoCaricato(const char* caricato, const char* nomeFile, ErrBuf& err, ErrBuf& avviso);

// Scrive in avviso se il pacchetto nuovo (la testa del suo testo) ha una versione piu bassa del file installato. Non blocca nulla.
void pacchettoAvviso(const char* installato, std::string_view nuovo, ErrBuf& avviso);
