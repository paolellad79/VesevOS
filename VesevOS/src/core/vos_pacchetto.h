// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_pacchetto.h
// Quello che un pacchetto (app, driver, script) dichiara di se stesso: nome, versione, autore, descrizione.
// Regola del 05/11/2026: le chiavi si scrivono in italiano (nome=, titolo=, versione=...), le vecchie inglesi si leggono ancora.
// Se nel file ci sono entrambe vale l'ultima riga; solo per il nome devono coincidere, altrimenti il file e rifiutato.
// Il file distribuito si chiama <nome>-<versione>.<est> (Berry) o <nome>-<CPU>-<versione>.<est> (ELF); sulla scheda resta il nome corto.
#pragma once
#include <stddef.h>
#include <string_view>

struct PacDati {
  char nome[25];
  char descrizione[33];
  char versione[11];
  char autore[25];
  char chi[6];
};

std::string_view pacChiave(std::string_view chiave);                 // chiave italiana -> nome interno; le altre restano come sono
bool pacVersioneOk(std::string_view v);                              // cifre e punti, da 1 a 10 caratteri
bool pacVersionePiuBassa(const char* nuova, const char* installata);  // numero per numero (1.10 > 1.9); false se una delle due manca
// Versioni del firmware: cifre e punti con una lettera finale facoltativa (1.8.17a viene dopo 1.8.17 e prima di 1.8.18).
#define PAC_VERSIONE_ILLEGGIBILE 2
int  pacConfrontaFirmware(const char* a, const char* b);               // -1 a piu vecchia, 0 uguali, 1 a piu nuova, PAC_VERSIONE_ILLEGGIBILE se una non si legge
void pacLeggi(std::string_view testo, PacDati& d);                   // legge la testa del file; i campi non dichiarati restano vuoti
bool pacNomeDaFile(std::string_view radice, char* out, size_t cap);  // toglie -<versione> e -<CPU> dal nome del file; false = non c'erano
