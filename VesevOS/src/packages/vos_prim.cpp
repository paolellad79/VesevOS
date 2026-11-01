// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_prim.cpp
// STRATO 6 (packages): il registro delle primitive (vedi vos_prim.h): tabelle, ricerche, contesto del modulo in esecuzione, copia dei testi.
// Nessuna primitiva qui: stanno in vos_prim_<gruppo>.cpp.
#include "vos_prim.h"

#if VOS_WITH_ELF
#include <string.h>

#define MAX_GRUPPI 16

static struct { const char* gruppo; const vos_prim_t* tab; int n; } g_gr[MAX_GRUPPI];
static int g_ng;

int vos_prim_registra(const char* gruppo, const vos_prim_t* tab, int n) {
  if (!gruppo || !tab || n <= 0 || g_ng >= MAX_GRUPPI) return -1;
  for (int i = 0; i < n; i++) {
    if (!tab[i].nome || !tab[i].fn || vos_prim_trova(tab[i].nome, nullptr)) return -1;   // niente nomi doppi
  }
  g_gr[g_ng].gruppo = gruppo;
  g_gr[g_ng].tab = tab;
  g_gr[g_ng].n = n;
  g_ng++;
  return 0;
}

int vos_prim_conta() {
  int t = 0;
  for (int g = 0; g < g_ng; g++) t += g_gr[g].n;
  return t;
}

const vos_prim_t* vos_prim_trova(const char* nome, const char** gruppo) {
  if (!nome) return nullptr;
  for (int g = 0; g < g_ng; g++) {
    for (int i = 0; i < g_gr[g].n; i++) {
      if (strcmp(g_gr[g].tab[i].nome, nome) != 0) continue;
      if (gruppo) *gruppo = g_gr[g].gruppo;
      return &g_gr[g].tab[i];
    }
  }
  return nullptr;
}

const vos_prim_t* vos_prim_trova_elf(const char* simbolo, const char** gruppo) {
  if (!simbolo || strncmp(simbolo, "vos_", 4) != 0) return nullptr;
  return vos_prim_trova(simbolo + 4, gruppo);
}

// La parola e una delle parole di use= (separate da spazi o virgole)?
static bool parolaInUse(const char* use, const char* parola) {
  if (!use || !parola || !*parola) return false;
  size_t gl = strlen(parola);
  const char* p = use;
  while (*p) {
    while (*p == ' ' || *p == ',') p++;
    const char* s = p;
    while (*p && *p != ' ' && *p != ',') p++;
    if ((size_t)(p - s) == gl && !strncmp(s, parola, gl)) return true;
  }
  return false;
}

int vos_prim_use_ha(const char* use, const char* gruppo) {
  if (!strcmp(gruppo, VOS_PRIM_GRUPPO_BASE)) return 1;
  return parolaInUse(use, gruppo) ? 1 : 0;
}

static bool (*g_funz)(const char*) = nullptr;
void vos_prim_funz_set(bool (*concessa)(const char* gruppo)) { g_funz = concessa; }

int vos_prim_permessa(const char* gruppo, const vos_prim_t* p, int ruolo, const char* use, int api) {
  if (api < p->da_api) return VOS_PRIM_NO_API;
  if (!vos_prim_use_ha(use, gruppo)) return VOS_PRIM_NO_USE;
  if (g_funz && !g_funz(gruppo)) return VOS_PRIM_NO_FUNZ;                 // l'utente ha tolto la funzionalita
  if (ruolo < p->ruolo_min) return VOS_PRIM_NO_RUOLO;
  return VOS_PRIM_OK;
}

// I moduli in esecuzione: un posto ciascuno, riconosciuto dal task. Copiato qui, cosi non dipende dal buffer di chi ha lanciato.
static struct Posto {
  void* chi;                        // identificatore del task (nullptr = posto libero)
  int ruolo;
  char use[VOS_PRIM_USE_MAX];
  char nome[32];                    // nome dell'app (vuoto per un driver)
  char* out; size_t cap, len;       // uscita testuale (facoltativa)
} g_esec[VOS_PRIM_ESEC_MAX];

static void* (*g_chi)() = nullptr;
static void* chiSono() { return g_chi ? g_chi() : (void*)1; }   // senza task distinti ce n'e uno solo

void vos_prim_task_set(void* (*chi)()) { g_chi = chi; }

static Posto* postoDi(void* chi) {
  for (int i = 0; i < VOS_PRIM_ESEC_MAX; i++) if (g_esec[i].chi == chi) return &g_esec[i];
  return nullptr;
}

int vos_prim_esec_entra(int ruolo, const char* use, const char* nome) {
  void* io = chiSono();
  if (postoDi(io)) return 0;                                  // questo task e gia dentro un modulo
  for (int i = 0; i < VOS_PRIM_ESEC_MAX; i++) {
    void* vuoto = nullptr;
    if (!__atomic_compare_exchange_n(&g_esec[i].chi, &vuoto, io, false, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE)) continue;
    g_esec[i].ruolo = ruolo;
    g_esec[i].out = nullptr; g_esec[i].cap = 0; g_esec[i].len = 0;
    if (!vos_prim_copia_testo(use ? use : "", g_esec[i].use, sizeof(g_esec[i].use))) g_esec[i].use[0] = 0;   // use= troppo lungo: nessun permesso
    if (!vos_prim_copia_testo(nome ? nome : "", g_esec[i].nome, sizeof(g_esec[i].nome))) g_esec[i].nome[0] = 0;
    return i + 1;
  }
  return 0;
}

void vos_prim_esec_esce(int posto) {
  Posto* p = posto > 0 && posto <= VOS_PRIM_ESEC_MAX ? &g_esec[posto - 1] : postoDi(chiSono());
  if (!p) return;
  p->use[0] = 0;
  p->nome[0] = 0;
  p->ruolo = ROLE_GUEST;
  p->out = nullptr;                                           // la lunghezza resta: chi ha il buffer la legge dopo la fine del modulo
  __atomic_store_n(&p->chi, (void*)nullptr, __ATOMIC_RELEASE);
}

const char* vos_prim_esec_nome() { Posto* p = postoDi(chiSono()); return p ? p->nome : ""; }

int vos_prim_esec_ruolo() { Posto* p = postoDi(chiSono()); return p ? p->ruolo : ROLE_GUEST; }

bool vos_prim_esec_ha(const char* parola) { Posto* p = postoDi(chiSono()); return p && parolaInUse(p->use, parola); }

void vos_prim_esec_uscita(int posto, char* buf, size_t cap) {
  if (posto < 1 || posto > VOS_PRIM_ESEC_MAX) return;
  Posto& p = g_esec[posto - 1];
  p.len = 0;
  if (buf && cap) buf[0] = 0;
  p.cap = buf ? cap : 0;
  p.out = buf;
}

size_t vos_prim_esec_scrivi(const char* testo) {
  Posto* p = postoDi(chiSono());
  if (!p || !p->out || !testo) return 0;
  size_t n = 0;
  while (testo[n] && p->len + n + 1 < p->cap) { p->out[p->len + n] = testo[n]; n++; }
  p->len += n;
  p->out[p->len] = 0;
  return n;
}

size_t vos_prim_esec_lunghezza(int posto) { return posto >= 1 && posto <= VOS_PRIM_ESEC_MAX ? g_esec[posto - 1].len : 0; }

bool vos_prim_copia_testo(const char* in, char* out, size_t cap) {
  if (!in || !out || cap == 0) return false;
  size_t n = 0;
  while (n < cap && in[n]) { out[n] = in[n]; n++; }
  if (n >= cap) { out[0] = 0; return false; }
  out[n] = 0;
  return true;
}

void vos_prim_gruppi_registra() {
  vos_prim_log_registra();
#if VOS_WITH_SD
  vos_prim_sd_registra();
#endif
  vos_prim_dev_registra();
  vos_prim_hw_registra();
  vos_prim_files_registra();
}
#endif
