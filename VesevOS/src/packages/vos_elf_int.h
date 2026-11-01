// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_elf_int.h
// STRATO 6 (packages): interno ai moduli ELF. Contesto di un carico e controlli condivisi tra vos_elf, vos_elf_check e vos_elf_syms.
#pragma once
#include "vos_elf_types.h"

#if VOS_WITH_ELF
#define VOS_ELF_SEZ_MAX 96          // massimo di sezioni accettate

// Chi sta caricando (vale solo mentre si tiene il blocco del driver).
struct vos_elf_ctx_t {
  int         ruolo;
  const char* use;
  int         api;
  int         motivo;               // 0 oppure VOS_ELF_E_* della prima primitiva rifiutata
  char        simbolo[32];
};

vos_elf_ctx_t* vos_elf_ctx();
uintptr_t vos_elf_sym_risolvi(const char* nome);     // livello C: lo usa il driver come risolutore
int vos_elf_ispeziona(const uint8_t* p, size_t len, const vos_elf_caps_t* c, const vos_elf_param_t* par, vos_elf_info_t* info);
#endif
