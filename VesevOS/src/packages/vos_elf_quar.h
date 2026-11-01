// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_elf_quar.h
// STRATO 6 (packages): QUARANTENA dei moduli ELF. Un modulo che fa cadere la scheda VOS_ELF_QUAR_N volte di fila (panico o watchdog
// mentre lui gira) non parte piu da solo: "vesevos-elf run" lo rifiuta finche l'Admin non lo toglie dalla quarantena.
// Come funziona, senza processi di sorveglianza: prima di eseguire un modulo il suo nome si scrive nell'elenco "in corso"; se finisce
// o viene fermato bene, il nome esce. All'avvio successivo, se l'elenco non e vuoto e l'ultimo riavvio e stato un guasto, quei moduli
// prendono un punto; al terzo punto di fila entrano in quarantena. Un riavvio voluto o uno spegnimento non danno punti.
// NON protegge dal crash: la scheda cade ancora (nessuna protezione di memoria). Evita solo di cadere di nuovo alla ripartenza.
// I dati stanno dietro un'interfaccia (VosElfQuarStore): sul firmware e la NVS (vos_elf_quar_nvs.cpp), nelle prove la memoria.
#pragma once
#include <stddef.h>
#include "../core/vos_common.h"

#if VOS_WITH_ELF
#define VOS_ELF_QUAR_N    3     // guasti di fila prima della quarantena
#define VOS_ELF_NOME_MAX  24    // lunghezza massima del nome di un modulo (senza estensione)

struct VosElfQuarStore {
  virtual bool get(const char* chiave, char* out, size_t cap) = 0;   // false = manca (out resta vuoto)
  virtual void put(const char* chiave, const char* valore) = 0;      // valore vuoto = cancella
  virtual ~VosElfQuarStore() {}
};

void vos_elf_quar_nvs_usa();                           // collega la NVS (vos_elf_quar_nvs.cpp)
bool vos_elf_nome_valido(const char* nome);               // 1-24 caratteri: minuscole, cifre, - e _
void vos_elf_quar_usa(VosElfQuarStore* s);                // dove si salva (una volta, all'avvio)
void vos_elf_quar_avvio(const char* nome);                // il modulo sta per girare
void vos_elf_quar_fine(const char* nome);                 // finito o fermato bene: azzera i punti
void vos_elf_quar_al_boot(bool riavvio_per_guasto);       // una volta, dopo vos_elf_quar_usa
bool vos_elf_quar_dentro(const char* nome);
bool vos_elf_quar_togli(const char* nome);                // false = non era in quarantena
int  vos_elf_quar_elenco(char out[][VOS_ELF_NOME_MAX + 1], int max);
#endif
