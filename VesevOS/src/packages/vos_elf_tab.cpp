// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_elf_tab.cpp (vedi vos_elf_tab.h)
#include "vos_elf_tab.h"

#if VOS_WITH_ELF
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace {
vos_elf_proc_t g_tab[VOS_ELF_RUN_MAX];     // statica: i moduli in corso non cambiano mai indirizzo
int            g_prossimoId = 1;
}

vos_elf_proc_t* vos_elf_tab_prendi(const char* nome, int* err) {
  for (int i = 0; i < VOS_ELF_RUN_MAX; i++) {
    if (g_tab[i].stato != VOS_ELF_P_LIBERO && !strcmp(g_tab[i].nome, nome)) { *err = VOS_ELF_E_GIA; return nullptr; }
  }
  for (int i = 0; i < VOS_ELF_RUN_MAX; i++) {
    vos_elf_proc_t* p = &g_tab[i];
    if (p->stato != VOS_ELF_P_LIBERO) continue;
    memset(p, 0, sizeof *p);
    p->id = g_prossimoId++;
    p->stato = VOS_ELF_P_ATTIVO;
    snprintf(p->nome, sizeof p->nome, "%s", nome);
    *err = VOS_ELF_OK;
    return p;
  }
  *err = VOS_ELF_E_PIENO;
  return nullptr;
}

void vos_elf_tab_rilascia(vos_elf_proc_t* p) {
  if (p) memset(p, 0, sizeof *p);          // stato = LIBERO
}

vos_elf_proc_t* vos_elf_tab_trova(const char* nome_o_id) {
  if (!nome_o_id || !nome_o_id[0]) return nullptr;
  char* fine = nullptr;
  long id = strtol(nome_o_id, &fine, 10);
  bool numero = fine && *fine == 0;
  for (int i = 0; i < VOS_ELF_RUN_MAX; i++) {
    vos_elf_proc_t* p = &g_tab[i];
    if (p->stato == VOS_ELF_P_LIBERO) continue;
    if (numero ? p->id == id : !strcmp(p->nome, nome_o_id)) return p;
  }
  return nullptr;
}

vos_elf_proc_t* vos_elf_tab_posto(int i) { return (i >= 0 && i < VOS_ELF_RUN_MAX) ? &g_tab[i] : nullptr; }

int vos_elf_tab_attivi() {
  int n = 0;
  for (int i = 0; i < VOS_ELF_RUN_MAX; i++) if (g_tab[i].stato != VOS_ELF_P_LIBERO) n++;
  return n;
}
#endif
