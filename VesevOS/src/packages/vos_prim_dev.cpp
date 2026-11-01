// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_prim_dev.cpp
// STRATO 6 (packages): gruppo "dev" del registro delle primitive (vedi vos_prim.h). Stessi nomi e semantica di dev_get e dev_set di Berry
// (vos_berry_dev.cpp) per le periferiche a classi di devices/vos_io. ELF: simboli vos_dev_get, vos_dev_set, vos_dev_list.
//
// REGOLE
//   - Il modulo NON sceglie ne applica pin: lavora solo su periferiche GIA configurate dall'utente (regola 6 dei driver). Nessuna
//     primitiva aggiunge, toglie o rinomina periferiche.
//   - Come in Berry, oltre al gruppo "dev" serve il NOME della periferica in use= (use=dev,rele1): "bmp1.p" si autorizza con "bmp1".
//   - dev_set solo se chi lancia e l'Admin (ruolo minimo alla registrazione); i limiti di ioSet (minimo 1 s tra due cambi del rele) restano.
// DIFFERENZE DA BERRY (scelte piu sicure, vedi piano-integrazione-elf.md, sezione 4.2)
//   - Berry usa dev_get/dev_set anche per i controlli e i dati dell'app (rtGet/rtSetApp): un modulo ELF non ha un'app, quindi qui solo
//     periferiche. Il valore e sempre un intero (niente testo).
//   - dev_list esiste solo per ELF (in Berry l'app conosce i nomi dal manifest): elenca SOLO le periferiche concesse in use=.
#include "vos_prim.h"

#if VOS_WITH_ELF
#include <stdio.h>
#include <string.h>
#include "../devices/vos_io.h"

#define NOME_MAX 24                       // "bmp1.p" e simili; piu lungo = nome non valido

namespace {

// Nome del modulo -> buffer (con tetto), poi la parte prima del punto per il controllo di use=.
bool leggiNome(const char* in, char* nome, char* base) {
  if (!vos_prim_copia_testo(in, nome, NOME_MAX) || !nome[0]) return false;
  size_t n = strcspn(nome, ".");
  memcpy(base, nome, n);
  base[n] = 0;
  return true;
}

// Legge il valore di una periferica: uscite 0/1, ingresso 0/1, analogico 0..4095, i2c in centesimi di grado (nome.p = pascal).
int primDevGet(const char* id, int* valore) {
  char nome[NOME_MAX], base[NOME_MAX];
  if (!valore || !leggiNome(id, nome, base)) return VOS_PRIM_E_ARG;
  if (!ioHas(nome)) return VOS_PRIM_E_NONTROVATO;
  if (!vos_prim_esec_ha(base)) return VOS_PRIM_E_USE;
  int x = 0;
  ErrBuf e;
  if (!ioGet(nome, x, e)) return VOS_PRIM_E_IO;
  *valore = x;
  return 0;
}

// Accende (valore diverso da 0) o spegne una uscita. Il permesso lo ha dato l'Admin che ha lanciato il modulo (ruolo minimo ADMIN).
int primDevSet(const char* id, int valore) {
  char nome[NOME_MAX], base[NOME_MAX];
  if (!leggiNome(id, nome, base)) return VOS_PRIM_E_ARG;
  if (!ioHas(nome)) return VOS_PRIM_E_NONTROVATO;
  if (!vos_prim_esec_ha(nome)) return VOS_PRIM_E_USE;
  ErrBuf e;
  return ioSet(nome, valore != 0, ROLE_ADMIN, e) ? 0 : VOS_PRIM_E_IO;
}

// Una riga "nome classe" per ogni periferica concessa, nel buffer del modulo (chiuso con zero). Ritorna quante, o VOS_PRIM_E_GRANDE se
// il buffer non basta (mai una risposta troncata).
int primDevList(char* out, int out_max) {
  if (!out || out_max < 1) return VOS_PRIM_E_ARG;
  int pos = 0, n = 0;
  out[0] = 0;
  for (int i = 0; i < ioCount(); i++) {
    const char* nome = ioNameAt(i);
    if (!nome[0] || !vos_prim_esec_ha(nome)) continue;
    int w = snprintf(out + pos, (size_t)(out_max - pos), "%s %s\n", nome, ioClassName(nome));
    if (w < 0 || w >= out_max - pos) { out[0] = 0; return VOS_PRIM_E_GRANDE; }
    pos += w;
    n++;
  }
  return n;
}

const vos_prim_t kDev[] = {
  { "dev_get",  (const void*)primDevGet,  "int vos_dev_get(const char *id, int *valore)", ROLE_OPER, 2 },
  { "dev_list", (const void*)primDevList, "int vos_dev_list(char *out, int out_max)", ROLE_OPER, 2 },
  { "dev_set",  (const void*)primDevSet,  "int vos_dev_set(const char *id, int valore)", ROLE_ADMIN, 2 },
};

}  // namespace

void vos_prim_dev_registra() {
  vos_prim_registra("dev", kDev, sizeof(kDev) / sizeof(kDev[0]));
}
#endif
