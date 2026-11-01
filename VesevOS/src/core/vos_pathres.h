// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_pathres.h
// Normalizzazione dei percorsi di shell e Berry (regole sulla microSD e sulle scorciatoie: vos_paths.h).
#pragma once
#include <stddef.h>

// Da un argomento (assoluto o relativo a cwd, un percorso pulito, "/" se vuota) al percorso pulito in out (cap almeno FS_PATH_MAX + 1).
// "." e le barre doppie o finali spariscono, ".." sale di un livello e non esce mai da "/", "~" da solo o all'inizio vale "/".
// Un argomento vuoto vale la cartella corrente. false se ci sono caratteri non ammessi (controllo, non ASCII, "\"), se il risultato supera
// FS_PATH_MAX caratteri o se fsCleanTo() lo rifiuta (per esempio "a..b"). Non guarda se il percorso esiste.
bool pathResolve(const char* cwd, const char* arg, char* out, size_t cap);
