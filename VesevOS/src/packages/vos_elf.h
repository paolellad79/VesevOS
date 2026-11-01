// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_elf.h
// STRATO 6 (packages), LIVELLO A dei moduli ELF (indipendente dal chip). UNICA API per i moduli ELF (.vae / .vde):
// valida, carica da buffer, esegui, scarica. Nessun include di ESP-IDF. Nessun #ifdef di scheda: cio che dipende dal chip sta in
// drivers/vos_drv_elf.* (livello B) e in boards/ (numeri). Le primitive che il modulo puo chiamare stanno in vos_prim.h (livello C).
// I moduli girano con tutti i diritti (nessuna protezione di memoria): servono solo per codice proprio o fidato. Spenti di fabbrica
// (VOS_WITH_ELF = 0 a compilazione; servizio "elf" spento a runtime, vedi vos_elf_svc.cpp).
// I comandi run/ps/kill/which e l installatore stanno in vos_elf_run.h, vos_elf_file.h e interfaces/vos_shell_elf.cpp (dalla 1.8.12).
#pragma once
#include "vos_elf_types.h"

#if VOS_WITH_ELF
int  vos_elf_init();                                   // una volta (dopo la registrazione delle primitive); la chiama il servizio "elf"
void vos_elf_abilita(bool on);                         // interruttore a runtime: spento = vos_elf_carica torna VOS_ELF_E_SPENTO
bool vos_elf_abilitato();
const vos_elf_caps_t* vos_elf_caps();                  // cosa offre questa scheda (copia statica aggiornata a ogni chiamata)
int  vos_elf_valida(const uint8_t* p, size_t len, const vos_elf_param_t* par, vos_elf_info_t* info);   // senza caricare, senza allocare
int  vos_elf_carica(const uint8_t* p, size_t len, const vos_elf_param_t* par, vos_elf_mod_t* m);
int  vos_elf_esegui(vos_elf_mod_t* m, int argc, char** argv, int* ritorno);   // nel task di chi chiama
int  vos_elf_scarica(vos_elf_mod_t* m);                // E_OCCUPATO se in uso; sicura anche dopo un carico fallito
void vos_elf_scarica_forza(vos_elf_mod_t* m);          // dopo aver fermato il task del modulo
void vos_elf_dettagli(const vos_elf_mod_t* m, vos_drv_elf_info_t* i);
const char* vos_elf_err_testo(int err);                // testo breve tradotto
const char* vos_elf_ultimo_simbolo();                  // ultimo simbolo rifiutato dal risolutore (solo diagnosi)
void elfServiceInit();                                 // vos_elf_svc.cpp: iscrive il servizio "elf" (spento di fabbrica); dopo la lettura della configurazione
#endif
