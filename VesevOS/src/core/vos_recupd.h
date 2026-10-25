// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_recupd.h
// Aggiornamento del RECOVERY da VesevOS (pagina web o shell), senza cavo.
// Il file arriva tutto in PSRAM e viene controllato PRIMA di toccare il recovery (intestazione ESP32-S3, segmenti, somma,
// SHA-256 dell'immagine, marchio del recovery, dimensione, SHA-256 facoltativo scritto dall'utente). Solo se va tutto bene
// il driver riscrive la partizione e la rilegge. Se il file e sbagliato il recovery vecchio resta com'e.
#pragma once
#include "vos_errbuf.h"
#include <stddef.h>
#include <stdint.h>

bool   recUpBegin(ErrBuf& err);                       // prepara la memoria (un caricamento alla volta)
bool   recUpAdd(const uint8_t* d, size_t n);          // aggiunge un pezzo; false = troppo grande (il resto viene ignorato)
void   recUpAbort();                                  // libera la memoria senza scrivere
// Controlla e scrive. sha = impronta attesa (64 cifre esadecimali, facoltativa). who = chi (per il registro).
// Se va bene: from/to = versione vecchia e nuova. Libera sempre la memoria.
bool   recUpFinish(const char* sha, const char* who, char* from, char* to, size_t verLen, ErrBuf& err);   // from/to: versione di prima e di dopo (verLen byte ciascuno, 16 bastano)
bool   recUpFromFile(const char* path, const char* sha, const char* who, char* from, char* to, size_t verLen, ErrBuf& err);   // da un file in memoria interna (shell)
const char* recVersion();                                // versione del recovery installato ("" = sconosciuta o vecchio senza marchio)
