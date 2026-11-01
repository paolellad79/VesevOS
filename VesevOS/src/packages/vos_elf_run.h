// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_elf_run.h
// STRATO 6 (packages): AVVIO IN SOTTOFONDO dei moduli ELF ("vesevos-elf run / ps / kill"). Ogni modulo ha il suo task (stack 8 KB, priorita
// bassa). Un piccolo supervisore (si crea da solo quando serve e sparisce quando non c'e piu nessuno) raccoglie i moduli finiti, ferma
// quelli oltre il tetto di tempo e quelli a cui e stato chiesto "kill", e li scarica. Tetti: VOS_ELF_RUN_MAX moduli insieme,
// VOS_ELF_RUN_RAM_MAX byte di RAM per modulo. Un modulo che cade (panico, watchdog) fa riavviare la scheda: per questo c'e la quarantena
// (vos_elf_quar.h). Senza protezione di memoria nient'altro e garantito. Prova di un modulo senza tabella: vos_elf_prova.h.
#pragma once
#include "vos_elf_tab.h"

#if VOS_WITH_ELF
#define VOS_ELF_RUN_RAM_MAX  (128 * 1024)   // RAM massima per modulo avviato con run (il tetto della scheda se e piu basso)
#define VOS_ELF_ULTIMI       4              // quanti moduli finiti ricorda "ps"

struct vos_elf_run_par_t {
  int         ruolo;          // ROLE_* di chi lancia
  const char* use;            // gruppi di primitive consentiti (nullptr = solo il gruppo base)
  int         argc;           // argv[0] = nome del file
  char**      argv;           // argv[argc] = nullptr
  uint32_t    tempo_max_ms;   // 0 = nessun tetto (il modulo gira finche non finisce o non lo si ferma)
  char*       uscita;         // facoltativo: buffer dove vos_print() del modulo scrive (comandi e widget dei driver); resta di chi lo da', valido finche il modulo non e finito
  size_t      uscita_cap;     // dimensione del buffer (zero compreso)
};

struct vos_elf_ps_t {
  int      id;
  char     nome[VOS_ELF_NOME_MAX + 1];
  bool     attivo;            // false = finito (voce degli ultimi)
  uint32_t secondi;           // da quanto gira, o quanto e girato
  uint32_t ram_stima;
  int      ritorno;           // solo se finito
  int      uscita;            // VOS_ELF_USCITA_*, solo se finito
  char     use[VOS_ELF_USE_MAX];   // gruppi concessi ("log sd")
};

// Avvia il modulo. img = il file (malloc): se torna 0 diventa del processo, che lo libera; altrimenti resta di chi chiama.
// Ritorna 0 o un VOS_ELF_E_*; *id = numero del processo.
int  vos_elf_run(const char* nome, uint8_t* img, size_t len, const vos_elf_run_par_t& par, int* id);
int  vos_elf_ps(vos_elf_ps_t* out, int max);          // prima gli attivi, poi gli ultimi finiti
int  vos_elf_ferma(const char* nome_o_id);            // 0 = fermato, VOS_ELF_E_NOME = non c'e, VOS_ELF_E_OCCUPATO = non risponde
void vos_elf_ferma_tutti();                           // allo spegnimento del servizio
int  vos_elf_ferma_uso(const char* gruppo);           // chiede di fermare i moduli che usano il gruppo (Funzionalita tolta); ritorna quanti
int  vos_elf_conta_uso(const char* gruppo);           // quanti moduli IN CORSO usano il gruppo
void vos_elf_ferma_aspetta();                         // aspetta (fino a 2 s) che i moduli fermati siano spariti
#endif
