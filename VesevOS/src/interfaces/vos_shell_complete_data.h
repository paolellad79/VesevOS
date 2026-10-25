// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_shell_complete_data.h
// INTERNO al completamento: le regole per le parole piu a fondo e i comandi con percorsi (vos_shell_complete_data.cpp). Nomi dei comandi e
// parole subito dopo il comando: tabella unica in vos_shell_cmdtab.cpp.
#pragma once
#include <stddef.h>

// Una regola per i sottocomandi: il comando cmd, alla parola numero idx (0 = il comando, 1 = la prima parola dopo), offre le parole di words
// (separate da spazi). prev: se non e nullptr, la parola idx-1 deve essere una di queste (separate da spazi).
struct SubRule {
  const char* cmd;
  int         idx;
  const char* prev;
  const char* words;
};

// Un comando con percorsi: le parole da first a last sono percorsi. dirsOnly = solo cartelle (cd, mkdir).
struct PathRule {
  const char* cmd;
  int         first;
  int         last;
  bool        dirsOnly;
  const char* opts;       // sigle e opzioni che il Tab propone quando la parola comincia con '-' (separate da spazi; nullptr = nessuna)
};

extern const SubRule     kSubRules[];
extern const size_t      kSubRulesN;
extern const PathRule    kPathRules[];
extern const size_t      kPathRulesN;
