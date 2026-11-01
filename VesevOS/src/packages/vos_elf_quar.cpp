// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_elf_quar.cpp (vedi vos_elf_quar.h)
// Tre elenchi di testo nella memoria permanente: "run" (moduli in esecuzione: "a,b"), "pt" (punti di guasto: "a=2,b=1"), "quar" ("a,b").
#include "vos_elf_quar.h"

#if VOS_WITH_ELF
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace {

const size_t LISTA_MAX = 208;           // sta in una voce NVS; basta per 8 nomi
VosElfQuarStore* g_s = nullptr;

void leggi(const char* chiave, char* lista) {
  lista[0] = 0;
  if (g_s) g_s->get(chiave, lista, LISTA_MAX);
}

void salva(const char* chiave, const char* lista) {
  if (g_s) g_s->put(chiave, lista);
}

// Inizio della voce del modulo nell'elenco (la voce e "nome" oppure "nome=punti"), nullptr se manca.
const char* trovaVoce(const char* lista, const char* nome) {
  size_t n = strlen(nome);
  const char* v = lista;
  while (*v) {
    const char* fine = strchr(v, ',');
    size_t len = fine ? (size_t)(fine - v) : strlen(v);
    const char* uguale = (const char*)memchr(v, '=', len);
    size_t lenNome = uguale ? (size_t)(uguale - v) : len;
    if (lenNome == n && !strncmp(v, nome, n)) return v;
    if (!fine) break;
    v = fine + 1;
  }
  return nullptr;
}

bool aggiungiVoce(char* lista, const char* voce) {
  size_t l = strlen(lista), n = strlen(voce);
  if (l + n + 2 > LISTA_MAX) return false;                 // elenco pieno
  if (l) lista[l++] = ',';
  memcpy(lista + l, voce, n + 1);
  return true;
}

void togliVoce(char* lista, const char* nome) {
  const char* v = trovaVoce(lista, nome);
  if (!v) return;
  const char* fine = strchr(v, ',');
  if (fine) memmove((char*)v, fine + 1, strlen(fine + 1) + 1);
  else if (v == lista) lista[0] = 0;
  else lista[(v - lista) - 1] = 0;                         // era l'ultima: via anche la virgola davanti
}

int puntiDi(const char* lista, const char* nome) {
  const char* v = trovaVoce(lista, nome);
  if (!v) return 0;
  const char* uguale = strchr(v, '=');
  const char* fine = strchr(v, ',');
  if (!uguale || (fine && uguale > fine)) return 0;
  return atoi(uguale + 1);
}

}  // namespace

bool vos_elf_nome_valido(const char* nome) {
  if (!nome || !nome[0]) return false;
  size_t n = strlen(nome);
  if (n > VOS_ELF_NOME_MAX) return false;
  for (size_t i = 0; i < n; i++) {
    char c = nome[i];
    if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_')) return false;
  }
  return true;
}

void vos_elf_quar_usa(VosElfQuarStore* s) { g_s = s; }

void vos_elf_quar_avvio(const char* nome) {
  char run[LISTA_MAX];
  leggi("run", run);
  if (trovaVoce(run, nome)) return;
  if (aggiungiVoce(run, nome)) salva("run", run);
}

void vos_elf_quar_fine(const char* nome) {
  char run[LISTA_MAX], pt[LISTA_MAX];
  leggi("run", run);
  if (trovaVoce(run, nome)) { togliVoce(run, nome); salva("run", run); }
  leggi("pt", pt);
  if (trovaVoce(pt, nome)) { togliVoce(pt, nome); salva("pt", pt); }
}

void vos_elf_quar_al_boot(bool riavvio_per_guasto) {
  char run[LISTA_MAX], pt[LISTA_MAX], quar[LISTA_MAX];
  leggi("run", run);
  if (!run[0]) return;
  salva("run", "");
  if (!riavvio_per_guasto) return;                         // riavvio voluto o spegnimento: nessun punto
  leggi("pt", pt);
  leggi("quar", quar);
  char copia[LISTA_MAX];
  snprintf(copia, sizeof copia, "%s", run);
  for (char* n = strtok(copia, ","); n; n = strtok(nullptr, ",")) {
    int p = puntiDi(pt, n) + 1;
    togliVoce(pt, n);
    if (p >= VOS_ELF_QUAR_N) {
      if (!trovaVoce(quar, n)) aggiungiVoce(quar, n);
      continue;
    }
    char voce[VOS_ELF_NOME_MAX + 8];
    snprintf(voce, sizeof voce, "%s=%d", n, p);
    aggiungiVoce(pt, voce);
  }
  salva("pt", pt);
  salva("quar", quar);
}

bool vos_elf_quar_dentro(const char* nome) {
  char quar[LISTA_MAX];
  leggi("quar", quar);
  return trovaVoce(quar, nome) != nullptr;
}

bool vos_elf_quar_togli(const char* nome) {
  char quar[LISTA_MAX], pt[LISTA_MAX];
  leggi("quar", quar);
  if (!trovaVoce(quar, nome)) return false;
  togliVoce(quar, nome);
  salva("quar", quar);
  leggi("pt", pt);
  togliVoce(pt, nome);
  salva("pt", pt);
  return true;
}

int vos_elf_quar_elenco(char out[][VOS_ELF_NOME_MAX + 1], int max) {
  char quar[LISTA_MAX];
  leggi("quar", quar);
  int n = 0;
  for (char* t = strtok(quar, ","); t && n < max; t = strtok(nullptr, ",")) snprintf(out[n++], VOS_ELF_NOME_MAX + 1, "%s", t);
  return n;
}
#endif
