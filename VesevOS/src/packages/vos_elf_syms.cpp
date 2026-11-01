// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_elf_syms.cpp
// STRATO 6 (packages), LIVELLO C dei moduli ELF: tabella dei simboli = REGISTRO DELLE PRIMITIVE (vos_prim.h).
// Il risolutore NON ha una lista sua: cerca nel registro, controlla api, use= e ruolo e consegna il puntatore.
// Elenco CHIUSO: oltre alle primitive esistono solo 3 funzioni di libc (memcpy, memset, strlen) e gli aiuti del compilatore del chip
// (livello B). Chiamato dal caricatore SOTTO il blocco del driver: legge il contesto di chi sta caricando (vos_elf_ctx).
#include "vos_elf_int.h"

#if VOS_WITH_ELF
#include <stdio.h>
#include <string.h>
#include "vos_prim.h"

static const struct { const char* nome; const void* ind; } kLibc[] = {
  { "memcpy", (const void*)memcpy },
  { "memset", (const void*)memset },
  { "strlen", (const void*)strlen },
};

// Ricorda solo il primo rifiuto di un carico: e quello che si dice all'utente.
static void nega(vos_elf_ctx_t* x, int motivo, const char* nome) {
  if (x->motivo) return;
  x->motivo = motivo;
  snprintf(x->simbolo, sizeof(x->simbolo), "%s", nome);
}

// Ruolo troppo basso: il modulo si carica lo stesso (un driver con comandi di lettura e di scrittura serve anche all'Operatore) e la funzione
// risponde "vietato" a ogni chiamata, senza fare nulla. Vale per tutte le funzioni che tornano un int; use= e api invece rifiutano il carico.
static int primNegata() { return VOS_PRIM_E_VIETATO; }

static int motivoDaPrim(int p) {
  return p == VOS_PRIM_NO_RUOLO ? VOS_ELF_E_RUOLO : p == VOS_PRIM_NO_USE ? VOS_ELF_E_USE : p == VOS_PRIM_NO_FUNZ ? VOS_ELF_E_FUNZ : VOS_ELF_E_API;
}

uintptr_t vos_elf_sym_risolvi(const char* nome) {
  vos_elf_ctx_t* x = vos_elf_ctx();
  if (!nome) return 0;
  for (size_t i = 0; i < sizeof(kLibc) / sizeof(kLibc[0]); i++) {
    if (!strcmp(kLibc[i].nome, nome)) return (uintptr_t)kLibc[i].ind;
  }
  uintptr_t aiuto = vos_drv_elf_aiuto(nome);
  if (aiuto) return aiuto;
  const char* gruppo = nullptr;
  const vos_prim_t* p = vos_prim_trova_elf(nome, &gruppo);
  if (!p) {
    nega(x, VOS_ELF_E_SIMBOLO, nome);
    return 0;
  }
  int r = vos_prim_permessa(gruppo, p, x->ruolo, x->use, x->api);
  if (r == VOS_PRIM_NO_RUOLO) return (uintptr_t)primNegata;
  if (r != VOS_PRIM_OK) {
    nega(x, motivoDaPrim(r), nome);
    return 0;
  }
  return (uintptr_t)p->fn;
}
#endif
