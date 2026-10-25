// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_shell_tar.h
// INTERNO al comando tar: le opzioni lette dalla riga e la funzione che CREA l'archivio (vos_shell_tarc.cpp).
// Lettura (-t, -x) e interpretazione della riga stanno in vos_shell_tar.cpp.
#pragma once
#include "vos_shell_arch.h"

namespace shx {

const size_t   TAR_BLOCK = 512;          // dimensione di un blocco ustar
const unsigned TAR_DEPTH_MAX = 2;        // cartelle dentro cartelle che tar -c scende: 3 livelli in tutto (ognuno tiene un elenco di nomi di circa 2,4 KB in memoria)

struct TarOpts {
  char mode = 0;                         // 'c' crea, 't' elenca, 'x' estrae
  bool verbose = false;
  bool overwrite = false;                // --overwrite: nell'estrazione sostituisce i file che ci sono gia (di fabbrica NO, diverso dal tar classico)
  Word archive;                          // -f
  Word dir;                              // -C
  int  firstOperand = 0;                 // indice della prima parola dopo le opzioni (0 = nessuna)
};

// tar -c: archive e base sono percorsi gia risolti. Scrive i messaggi da se'.
void tarCreate(ArchCtx& c, const TarOpts& op, const char* line, const char* archive, const char* base);

}  // namespace shx
