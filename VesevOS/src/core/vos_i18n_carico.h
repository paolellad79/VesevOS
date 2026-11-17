// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_i18n_carico.h
// Caricamento dei file di lingua in /lang: in questa cartella entrano solo "<codice>.vld" e "<codice>.vlp".
// Il file si scrive prima in "<percorso>.tmp~", si controlla e solo dopo prende il posto di quello installato
// (stesso schema dei pacchetti: se il nuovo non e valido, quello installato resta intatto).
#pragma once
#include <stddef.h>
#include "vos_errbuf.h"

enum LangFileKind {
  LANGF_FUORI = 0,   // il percorso non e dentro /lang
  LANGF_VLD = 1,     // dizionario del firmware (obbligatorio per installare una lingua)
  LANGF_VLP = 2,     // frasi della pagina, contenuto gzip (facoltativo)
  LANGF_NO = 3       // dentro /lang ma con nome o posizione non ammessi
};

// percorso = percorso gia pulito (fsCleanTo). Per VLD e VLP scrive in code il codice della lingua.
int    langFileKind(const char* percorso, char* code, size_t cap);
size_t langFileMax(int kind);                                  // dimensione massima del file, 0 se il tipo non ha tetto

// Controlla il file (intestazione "VOSL" per il .vld, intestazione gzip 1f 8b 08 per il .vlp, dimensione).
bool   langFileControlla(const char* percorso, int kind, ErrBuf& err);

// Dopo il caricamento: controlla "caricato" (il .tmp~), lo mette al posto di "finale" in un passo solo. Se e il .vld della
// lingua in uso la rilegge. false = rifiutato, il temporaneo e gia tolto e err dice perche.
bool   langFileCaricato(const char* caricato, const char* finale, int kind, const char* code, ErrBuf& err);
