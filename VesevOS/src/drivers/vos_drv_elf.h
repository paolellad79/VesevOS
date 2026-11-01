// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_drv_elf.h
// STRATO 1 (driver), LIVELLO B dei moduli ELF (dipende dal chip). Unico file che parla con il caricatore ELF e con la memoria eseguibile.
// Offre solo PRIMITIVE: carica da buffer, scarica, blocco, descrizione di cosa offre la scheda. Nessuna regola di ruolo o di pacchetto qui:
// le regole stanno in packages/vos_elf (livello A) e packages/vos_elf_syms (livello C). I numeri della scheda stanno in boards/.
// Con VOS_WITH_ELF = 0 il driver non c'e.
#pragma once
#include <stdint.h>
#include <stddef.h>
#include "../boards/vos_packages.h"

#if VOS_WITH_ELF
enum { VOS_ELF_ARCH_XTENSA = 1, VOS_ELF_ARCH_RISCV = 2 };

// Cosa offre la scheda (letto dal livello A per rifiutare i moduli che non ci stanno).
struct vos_elf_caps_t {
  uint8_t  arch;             // VOS_ELF_ARCH_XTENSA / VOS_ELF_ARCH_RISCV
  uint8_t  little_endian;    // 1
  uint8_t  abi;              // 1 Xtensa con finestre di registri, 2 RISC-V ilp32 soft
  uint8_t  ha_psram;         // PSRAM presente ADESSO (misurata, non solo prevista dalla scheda)
  uint8_t  esegue_da_psram;  // il codice del modulo puo stare in PSRAM
  uint8_t  text_esatto;      // il caricatore gestisce un .text non multiplo di 4 (patch 1)
  uint16_t machine;          // e_machine accettato
  uint16_t api_version;      // versione dell'API delle primitive (la mette il livello A)
  uint32_t ram_max_modulo;   // tetto di RAM per un modulo
  uint32_t file_max;         // tetto della dimensione del file ELF
};

struct vos_drv_elf;                                     // opaco
typedef struct vos_drv_elf vos_drv_elf_t;
typedef int (*vos_drv_elf_entry_t)(int argc, char** argv);
typedef uintptr_t (*vos_drv_elf_resolver_t)(const char* nome);

// Esiti di vos_drv_elf_carica
enum { VOS_DRV_ELF_OK = 0, VOS_DRV_ELF_MEM = 1, VOS_DRV_ELF_SIMBOLO = 2, VOS_DRV_ELF_ALTRO = 3 };

struct vos_drv_elf_info_t {
  uintptr_t entry;           // indirizzo (eseguibile) dell'ingresso
  uintptr_t text_addr;       // dove sta il codice in memoria
  uint32_t  text_size;
  uint32_t  sec_size[5];     // text, bss, data, rodata, data.rel.ro
};

int       vos_drv_elf_init(vos_drv_elf_resolver_t risolutore);   // una volta: blocco statico (nessuna allocazione) e risolutore dei simboli
int       vos_drv_elf_lock(uint32_t ms);                         // 0 = ottenuto
void      vos_drv_elf_unlock();
void      vos_drv_elf_caps(vos_elf_caps_t* c);
int       vos_drv_elf_flags_ok(uint32_t e_flags);                // controlli di intestazione propri del chip (ABI)
uintptr_t vos_drv_elf_aiuto(const char* nome);                   // aiuti del compilatore che dipendono dal chip (es. divisione float)
int       vos_drv_elf_carica(const uint8_t* p, size_t n, vos_drv_elf_t** h, vos_drv_elf_entry_t* entry);
void      vos_drv_elf_scarica(vos_drv_elf_t* h);                 // sempre sicuro, anche con h == nullptr
void      vos_drv_elf_info(const vos_drv_elf_t* h, vos_drv_elf_info_t* i);
#endif
