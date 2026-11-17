// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_i18n_fmt_rule.h
// Regola PURA: un testo tradotto puo servire da formato di printf solo se ha gli STESSI specificatori dell'italiano originale
// (stesso numero, stesso ordine, stesso tipo e stessa lunghezza). Un file di lingua sbagliato o manomesso (%n, un %s in piu, %d al posto
// di %s) altrimenti farebbe leggere o scrivere memoria a caso. Se non torna, tr() restituisce il testo italiano.
// Nessuna memoria: due puntatori che avanzano insieme, pochi byte di stack. Provata sul computer: build/tests/lingue/prova_formati.cpp.
// Specificatori ammessi: %% e %[flag][larghezza][.precisione][hh|h|l|ll|z|j|t|L]{d i u x X o c s p f F e E g G a A}.
// Un '%' come ultimo carattere del testo e testo normale. Vietati (testo non valido): %n, '*' in larghezza o precisione, parametri posizionali (%1$s), qualunque altro carattere dopo '%'.
#pragma once
#include <stddef.h>

struct FmtSpec {
  char tipo;      // classe: d (con segno), u (senza segno), c, s, p, f (virgola mobile)
  char lung;      // 0 nessuna, 'H' hh, 'h', 'l', 'L' ll, 'z', 'j', 't', 'D' L (long double)
};

enum FmtEsito { FMT_FINE = 0, FMT_SPEC = 1, FMT_ERRORE = 2 };

// Legge il prossimo specificatore da *p (salta il testo normale e i "%%") e avanza *p. FMT_FINE = niente altro.
static inline FmtEsito fmtProssimo(const char** p, FmtSpec* out) {
  const char* s = *p;
  for (;;) {
    while (*s && *s != '%') s++;
    if (!*s) { *p = s; return FMT_FINE; }
    s++;                                               // dopo il '%'
    if (*s == '%') { s++; continue; }                  // "%%": una percentuale, nessun argomento
    if (!*s) { *p = s; return FMT_FINE; }              // '%' come ultimo carattere ("Uso%"): testo normale, printf non legge nulla
    break;
  }
  while (*s == '-' || *s == '+' || *s == ' ' || *s == '#' || *s == '0') s++;
  int cifre = 0;
  while (*s >= '0' && *s <= '9') { s++; if (++cifre > 4) { *p = s; return FMT_ERRORE; } }
  if (*s == '.') {
    s++;
    cifre = 0;
    while (*s >= '0' && *s <= '9') { s++; if (++cifre > 4) { *p = s; return FMT_ERRORE; } }
  }
  char lung = 0;
  switch (*s) {
    case 'h': s++; if (*s == 'h') { s++; lung = 'H'; } else lung = 'h'; break;
    case 'l': s++; if (*s == 'l') { s++; lung = 'L'; } else lung = 'l'; break;
    case 'z': s++; lung = 'z'; break;
    case 'j': s++; lung = 'j'; break;
    case 't': s++; lung = 't'; break;
    case 'L': s++; lung = 'D'; break;
    default: break;
  }
  char tipo = 0;
  switch (*s) {
    case 'd': case 'i': tipo = 'd'; break;
    case 'u': case 'x': case 'X': case 'o': tipo = 'u'; break;
    case 'c': tipo = 'c'; break;
    case 's': tipo = 's'; break;
    case 'p': tipo = 'p'; break;
    case 'f': case 'F': case 'e': case 'E': case 'g': case 'G': case 'a': case 'A': tipo = 'f'; break;
    default: *p = s; return FMT_ERRORE;                // include %n, '*', '$', fine del testo dopo '%'
  }
  *p = s + 1;
  out->tipo = tipo;
  out->lung = lung;
  return FMT_SPEC;
}

// vero se 'tradotto' ha gli stessi specificatori di 'italiano' (anche nessuno). Un testo con uno specificatore vietato non e mai valido.
static inline bool i18nFormatiUguali(const char* italiano, const char* tradotto) {
  if (!italiano || !tradotto) return false;
  const char* a = italiano;
  const char* b = tradotto;
  for (;;) {
    FmtSpec sa = {0, 0}, sb = {0, 0};
    FmtEsito ea = fmtProssimo(&a, &sa);
    FmtEsito eb = fmtProssimo(&b, &sb);
    if (eb == FMT_ERRORE) return false;
    if (ea == FMT_ERRORE) return false;                // un originale storto non si confronta: si lascia il testo com'e (vedi tr())
    if (ea == FMT_FINE && eb == FMT_FINE) return true;
    if (ea != eb) return false;                        // uno ha uno specificatore in piu
    if (sa.tipo != sb.tipo || sa.lung != sb.lung) return false;
  }
}
