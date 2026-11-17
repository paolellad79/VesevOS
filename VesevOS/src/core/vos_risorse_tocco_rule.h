// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_risorse_tocco_rule.h
// Regole PURE per accorgersi di chi scrive dentro /usr/share/vesevos (decisione dell'utente del 16/11/2026: solo un avviso nel registro di audit,
// nessun blocco). Nessuna dipendenza dal chip: si prova sul computer (build/tests/risorse-file/prova_tocco.cpp).
#pragma once
#include <stddef.h>
#include <string.h>

#define RISTOCCO_DIR "/usr/share/vesevos"

// Il percorso e la cartella delle risorse o sta dentro? (percorso pulito, assoluto). Non basta il prefisso: "/usr/share/vesevos2" non e dentro.
static inline bool risToccoDentro(const char* p) {
  if (!p) return false;
  const size_t n = sizeof(RISTOCCO_DIR) - 1;
  return strncmp(p, RISTOCCO_DIR, n) == 0 && (p[n] == 0 || p[n] == '/');
}

// Il temporaneo di una scrittura atomica ("<nome>.tmp~"): non conta, conta lo scambio finale.
static inline bool risToccoTemporaneo(const char* p) {
  size_t n = p ? strlen(p) : 0;
  return n >= 5 && strcmp(p + n - 5, ".tmp~") == 0;
}

// La cartella stessa, senza nome di file: crearla non e un'alterazione di un file.
static inline bool risToccoLaCartella(const char* p) {
  return risToccoDentro(p) && (p[sizeof(RISTOCCO_DIR) - 1] == 0 || p[sizeof(RISTOCCO_DIR)] == 0);
}

// Questa azione del file system va segnalata? azione: w a r n m d (vedi drvFsGancioScrittura). Per 'n' (p = origine, p2 = destinazione):
// vale se uno dei due e un file vero di questa cartella (un temporaneo che diventa il file vero conta tramite la destinazione).
static inline bool risToccoDaSegnalare(char azione, const char* p, const char* p2) {
  auto vero = [](const char* x) { return risToccoDentro(x) && !risToccoTemporaneo(x) && !risToccoLaCartella(x); };
  if (azione == 'n') return vero(p) || vero(p2);
  if (azione == 'm' || azione == 'd') return risToccoDentro(p) && !risToccoLaCartella(p);
  return vero(p);
}

// Nome del file senza cartelle, ripulito per il registro: solo caratteri stampabili ASCII, al massimo n-1 caratteri. Mai contenuti di file.
static inline void risToccoNome(const char* p, char* out, size_t n) {
  if (!n) return;
  const char* sl = p ? strrchr(p, '/') : nullptr;
  const char* s = sl ? sl + 1 : (p ? p : "");
  size_t k = 0;
  for (; s[k] && k + 1 < n; k++) out[k] = (s[k] >= 32 && s[k] < 127) ? s[k] : '?';
  out[k] = 0;
}
