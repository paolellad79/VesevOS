// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_elf_types.h
// STRATO 6 (packages), LIVELLO A dei moduli ELF: tipi dell'API (errori, parametri, modulo caricato). Nessun include di ESP-IDF.
// Separati da vos_elf.h cosi che i controlli (vos_elf_check, vos_elf_syms) li usano senza includere l'API intera.
#pragma once
#include <stdint.h>
#include <stddef.h>
#include "../drivers/vos_drv_elf.h"
#include "vos_prim.h"                // VOS_PRIM_USE_MAX

#if VOS_WITH_ELF
#define VOS_ELF_API_VERSION 4               // versione della tabella delle primitive (livello C): 1 = base, log; 2 = anche sd, dev; 3 = anche gpio, i2c, spi, time, print; 4 = anche files (data_dir, data_write, data_read, data_rm)

// Tutti gli errori in un solo elenco: un file malformato non manda mai in crash, torna uno di questi codici.
enum vos_elf_err_t {
  VOS_ELF_OK = 0,
  VOS_ELF_E_ARG = -1,        // argomento nullo o non valido
  VOS_ELF_E_NOINIT = -2,     // vos_elf_init non chiamata
  VOS_ELF_E_CORTO = -3,      // file troncato o piu corto di quanto dice l'intestazione
  VOS_ELF_E_MAGIC = -4,      // non e un ELF
  VOS_ELF_E_CLASSE = -5,     // non 32 bit little-endian versione 1
  VOS_ELF_E_MACCHINA = -6,   // macchina diversa da quella del chip
  VOS_ELF_E_TIPO = -7,       // non eseguibile/condiviso
  VOS_ELF_E_GRANDE = -8,     // file oltre il tetto
  VOS_ELF_E_ALLINEA = -9,    // buffer non allineato a 4, o .rodata sovrapposta a .text con un caricatore senza la patch 1
  VOS_ELF_E_STRUTTURA = -10, // tabelle delle sezioni incoerenti
  VOS_ELF_E_RAM = -11,       // somma delle sezioni oltre il tetto di RAM
  VOS_ELF_E_ARCH = -12,      // il manifest chiede un'altra architettura
  VOS_ELF_E_CAPS = -13,      // il manifest chiede cio che la scheda non ha (PSRAM)
  VOS_ELF_E_API = -14,       // versione API chiesta piu nuova di quella del firmware, o primitiva piu nuova
  VOS_ELF_E_RUOLO = -15,     // primitiva negata dal ruolo di chi lancia
  VOS_ELF_E_USE = -16,       // primitiva fuori da use= del pacchetto
  VOS_ELF_E_SIMBOLO = -17,   // simbolo non nell'elenco chiuso
  VOS_ELF_E_MEM = -18,       // memoria esaurita
  VOS_ELF_E_CARICATORE = -19,// altro errore del caricatore
  VOS_ELF_E_OCCUPATO = -20,  // blocco non ottenuto, o modulo in uso
  VOS_ELF_E_INGRESSO = -21,  // punto di ingresso mancante o fuori da .text
  VOS_ELF_E_ABI = -22,       // flag di intestazione non adatte al chip
  VOS_ELF_E_SPENTO = -23,    // interruttore dei moduli ELF spento (servizio "elf")
  VOS_ELF_E_PIENO = -24,     // gia in esecuzione il massimo di moduli (VOS_ELF_RUN_MAX)
  VOS_ELF_E_GIA = -25,       // un modulo con questo nome sta gia girando
  VOS_ELF_E_QUARANTENA = -26,// il modulo ha fatto cadere la scheda troppe volte di fila (vos_elf_quar.h)
  VOS_ELF_E_NOME = -27,      // nome del modulo non valido, o modulo non trovato
  VOS_ELF_E_FILE = -28,      // file non leggibile o vuoto
  VOS_ELF_E_PERCORSO = -29,  // percorso non ammesso per i moduli
  VOS_ELF_E_FUNZ = -30       // il modulo chiede una funzionalita che l'utente ha tolto (Sistema > Funzionalita)
};

// Dal manifest del pacchetto e da chi lancia. Campi a 0/nullptr = "nessuna richiesta" (tranne use: nullptr = solo gruppo base).
struct vos_elf_param_t {
  int         ruolo;         // ROLE_* di chi lancia
  const char* use;           // use= del manifest: gruppi di primitive consentiti, es. "log sd dev"
  uint16_t    api;           // api= del manifest (0 = quella corrente)
  uint8_t     arch;          // arch= del manifest (0 = qualsiasi; altrimenti VOS_ELF_ARCH_*)
  uint8_t     serve_psram;   // il manifest dichiara di avere bisogno della PSRAM
  uint32_t    ram_max;       // tetto di RAM per questo modulo (0 = quello della scheda)
  uint32_t    file_max;      // tetto del file (0 = quello della scheda)
};

struct vos_elf_info_t {
  uint32_t ram_stima;          // somma delle sezioni ALLOC con i margini di allineamento
  uint32_t entry_vaddr;        // e_entry del file
  uint32_t n_sezioni;
  uint8_t  rodata_sovrapposto; // .rodata subito dopo un .text non multiplo di 4
};

struct vos_elf_mod_t {
  vos_drv_elf_t*      h;       // gestito dal driver
  vos_drv_elf_entry_t entry;
  uint32_t            ram_stima;
  volatile int        in_uso;  // 1 mentre vos_elf_esegui e in corso
  int                 ruolo;   // ROLE_* di chi ha lanciato e use= del manifest: le primitive li leggono mentre il modulo gira
  char                use[VOS_PRIM_USE_MAX];
  int                 posto;   // posto nel registro dei moduli in esecuzione (vos_prim_esec_*)
  char                nome[32];  // nome dell'app (vuoto per un driver .vde): serve alle primitive vos_data_* per trovare la cartella dei dati
  char*               uscita;  // facoltativo: buffer dove print() del modulo scrive (comandi e widget dei driver), messo da chi lancia
  size_t              uscita_cap, uscita_len;
};
#endif
