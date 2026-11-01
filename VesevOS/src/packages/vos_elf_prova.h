// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_elf_prova.h
// STRATO 6 (packages): PROVA di un modulo ELF in un colpo solo, per il comando "vesevos-elf test" (interfaces/vos_shell_elf.cpp).
// Valida, carica, esegue in un task dedicato (tetto di tempo), scarica, e misura tempo e memoria prima/durante/dopo.
// NON e un gestore di processi: un solo modulo alla volta, niente elenco, niente supervisore (vedi piano-integrazione-elf.md, 7b).
// Se il modulo non finisce entro il tetto di tempo il task viene eliminato e il modulo scaricato a forza: puo lasciare in giro
// risorse prese dal modulo (blocchi, memoria che aveva allocato per se). Senza protezione di memoria nient'altro e garantito.
#pragma once
#include "vos_elf_types.h"

#if VOS_WITH_ELF
enum vos_elf_fase_t { VOS_ELF_FASE_VALIDA = 1, VOS_ELF_FASE_CARICA = 2, VOS_ELF_FASE_ESEGUI = 3, VOS_ELF_FASE_FINITA = 4 };

struct vos_elf_prova_par_t {
  int         ruolo;         // ROLE_* di chi lancia
  const char* use;           // gruppi di primitive consentiti ("log" ...); nullptr = solo il gruppo base
  int         argc;
  char**      argv;          // argv[argc] deve essere nullptr
  uint32_t    tempo_max_ms;  // tetto di tempo per l'esecuzione
};

struct vos_elf_prova_ris_t {
  int      fase;             // ultima fase raggiunta (vos_elf_fase_t); se err != 0 e la fase in cui e fallita
  int      err;              // 0 oppure VOS_ELF_E_* (fasi valida e carica)
  int      ritorno;          // valore restituito dal main del modulo (solo se finito)
  bool     scaduto;          // tempo scaduto: task eliminato, modulo scaricato a forza
  uint32_t ram_stima;        // stima della RAM del modulo (somma delle sezioni)
  uint32_t ms_carica;
  uint32_t ms_esegui;
  uint32_t ram_prima, ram_caricato, ram_dopo;        // RAM interna libera (byte): prima, a modulo caricato, dopo lo scarico
  uint32_t psram_prima, psram_caricato, psram_dopo;  // idem PSRAM (0 se la scheda non ne ha)
  char     simbolo[32];      // se err e un rifiuto del risolutore: la funzione chiesta dal modulo e negata
};

// buf: il file ELF intero (allineato a 4, come lo da malloc). Ritorna 0 se il modulo e stato eseguito fino in fondo (anche con ritorno
// diverso da 0, o per tempo scaduto: lo dicono i campi di ris); altrimenti il VOS_ELF_E_* che ha fermato la prova (uguale a ris.err).
int vos_elf_prova(const uint8_t* buf, size_t len, const vos_elf_prova_par_t& par, vos_elf_prova_ris_t& ris);
#endif
