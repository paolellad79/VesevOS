// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_license.h
// Testi legali e licenze, memorizzati nel firmware (flash). Generati da tools/mklicense.py.
#pragma once
#include <stddef.h>

int         licCount();
const char* licId(int i);            // "notice", "gpl3", "apache2", "mit", "bsd3"
const char* licTitle(int i);
const char* licText(int i);          // testo in flash, terminato da 0
size_t      licSize(int i);              // lunghezza del testo originale
size_t      licZ(int i);                 // se > 0 il testo in flash e compresso (gzip) e questa e la sua lunghezza
int         licFind(const char* id);     // -1 se non esiste (maiuscole e minuscole uguali)
#define LIC_JSON_MAX 1024                // capacita che basta per l'elenco dei documenti (la prova sul computer lo controlla)
size_t      licListJson(char* out, size_t cap);   // [{"id":..,"title":..,"size":..}]; ritorna i caratteri scritti, tronca se non ci sta
size_t      licIds(char* out, size_t cap);        // "notice, gpl3, ..."; stesso contratto
