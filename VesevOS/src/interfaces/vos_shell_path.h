// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_shell_path.h
// Percorsi della shell: da un argomento (assoluto o relativo alla cartella corrente) al percorso pulito di LittleFS.
#pragma once
#include <stddef.h>

// Risolve arg partendo dalla cartella corrente cwd (un percorso pulito, "/" se vuota) e scrive il risultato in out (cap almeno FS_PATH_MAX + 1).
// Regole: "." e le barre doppie o finali spariscono, ".." sale di un livello e non esce mai da "/", "~" da solo o all'inizio vale "/".
// Un argomento vuoto vale la cartella corrente. Ritorna false se l'argomento ha caratteri non ammessi (controllo, non ASCII, "\"),
// se il risultato supera FS_PATH_MAX caratteri o se fsCleanTo() lo rifiuta (per esempio "a..b"). Non guarda se il percorso esiste
// e NON decide i permessi: file di sistema e protezioni restano nelle funzioni di vos_files, che lavorano sul percorso gia risolto.
bool shellResolvePath(const char* cwd, const char* arg, char* out, size_t cap);
