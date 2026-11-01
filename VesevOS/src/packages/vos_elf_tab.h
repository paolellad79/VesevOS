// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_elf_tab.h
// STRATO 6 (packages): la TABELLA dei moduli ELF in esecuzione ("vesevos-elf ps"). Solo posti e nomi: non crea task, non carica niente
// (lo fa vos_elf_run.cpp), cosi si prova sul computer. Chi la usa prende il blocco di vos_elf_run prima di toccarla.
#pragma once
#include "vos_elf_types.h"
#include "vos_elf_quar.h"      // VOS_ELF_NOME_MAX

#if VOS_WITH_ELF
#define VOS_ELF_RUN_MAX      4        // moduli in esecuzione insieme (tetto prudente: ognuno ha il suo task da 8 KB di stack)
#define VOS_ELF_ARGV_MAX     8
#define VOS_ELF_ARGBUF       200
#define VOS_ELF_USE_MAX      64       // gruppi di primitive concessi al modulo (uguale a VOS_PRIM_USE_MAX)

enum { VOS_ELF_P_LIBERO = 0, VOS_ELF_P_ATTIVO = 1 };
enum { VOS_ELF_USCITA_NORMALE = 0, VOS_ELF_USCITA_FERMATO = 1, VOS_ELF_USCITA_SCADUTO = 2 };

struct vos_elf_proc_t {
  int            id;                         // 1, 2, 3... non si riusa subito
  int            stato;                      // VOS_ELF_P_*
  char           nome[VOS_ELF_NOME_MAX + 1];
  int            ruolo;
  char           use[VOS_ELF_USE_MAX];       // gruppi concessi ("log sd")
  uint32_t       inizio_ms;
  uint32_t       tempo_max_ms;               // 0 = nessun tetto
  uint32_t       ram_stima;
  int            ritorno;
  vos_elf_mod_t  mod;
  uint8_t*       img;                        // il file ELF, tenuto finche il modulo gira
  int            argc;
  char*          argv[VOS_ELF_ARGV_MAX + 2];
  char           argBuf[VOS_ELF_ARGBUF];
  void*          task;                       // TaskHandle_t
  volatile bool  finito;                     // il main del modulo e tornato
  volatile bool  da_fermare;                 // "kill" chiesto
};

// Prende un posto per il modulo. nullptr con *err = VOS_ELF_E_PIENO (posti finiti) o VOS_ELF_E_GIA (stesso nome gia attivo).
vos_elf_proc_t* vos_elf_tab_prendi(const char* nome, int* err);
void            vos_elf_tab_rilascia(vos_elf_proc_t* p);
vos_elf_proc_t* vos_elf_tab_trova(const char* nome_o_id);   // un numero = id, altrimenti il nome
vos_elf_proc_t* vos_elf_tab_posto(int i);                   // 0..VOS_ELF_RUN_MAX-1, anche libero
int             vos_elf_tab_attivi();
#endif
