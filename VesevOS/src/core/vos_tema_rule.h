// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_tema_rule.h
// Regole del TEMA della pagina: un foglio di stile (/etc/tema.css) che l'amministratore carica SENZA cambiare il firmware e che la pagina applica sopra
// a quello di fabbrica. Un tema puo cambiare solo l'aspetto (colori, raggi, forma dei bottoni, caratteri di sistema). Il file e rifiutato per
// intero se e piu grande di TEMA_MAX, se contiene byte nulli o se contiene una di queste cose: barra rovesciata (le sequenze di escape
// nasconderebbero le parole vietate), '<', "@import", "url(", "image-set(", "src(", "expression(", "javascript:", "-moz-binding". Cosi un tema non
// puo chiedere nulla a un altro indirizzo ne caricare caratteri o immagini: i dati della pagina non escono. Controllo a pezzi, senza dipendenze:
// provato sul computer (tools/hosttest/prova_tema.sh). Chi lo usa: interfaces/vos_web_tema.cpp.
#pragma once
#include <stddef.h>
#include <stdint.h>

#define TEMA_PERCORSO "/etc/tema.css"
#define TEMA_MAX 32768                    // byte al massimo di un tema

enum TemaEsito : uint8_t { TEMA_OK = 0, TEMA_VUOTO, TEMA_GRANDE, TEMA_NUL, TEMA_VIETATO };

struct TemaScan {
  char      fin[12];                      // ultimi byte letti, in minuscolo (la parola vietata piu lunga ne ha 12)
  uint8_t   n;                            // quanti ne sono validi
  size_t    tot;                          // byte letti finora
  TemaEsito e;                            // esito finora (TEMA_OK finche non si trova qualcosa)
};

inline void temaScanAvvia(TemaScan& s) {
  s.n = 0;
  s.tot = 0;
  s.e = TEMA_OK;
  for (unsigned i = 0; i < sizeof s.fin; i++) s.fin[i] = 0;
}

inline bool temaFinisceCon(const TemaScan& s, const char* p) {
  size_t l = 0;
  while (p[l]) l++;
  if (l > s.n) return false;
  for (size_t i = 0; i < l; i++)
    if (s.fin[s.n - l + i] != p[i]) return false;
  return true;
}

// Un pezzo di file. Dopo il primo errore non cambia piu l'esito: chi chiama si ferma.
inline TemaEsito temaScanDai(TemaScan& s, const char* t, size_t len) {
  static const char* const kVietate[] = {"@import", "url(", "image-set(", "src(", "expression(", "javascript:", "-moz-binding"};
  for (size_t i = 0; i < len && s.e == TEMA_OK; i++) {
    char c = t[i];
    if (++s.tot > TEMA_MAX) { s.e = TEMA_GRANDE; break; }
    if (c == 0) { s.e = TEMA_NUL; break; }
    if (c == '\\' || c == '<') { s.e = TEMA_VIETATO; break; }
    if (c >= 'A' && c <= 'Z') c = (char)(c + 32);
    if (s.n == sizeof s.fin) {
      for (unsigned k = 1; k < sizeof s.fin; k++) s.fin[k - 1] = s.fin[k];
      s.n--;
    }
    s.fin[s.n++] = c;
    for (unsigned k = 0; k < sizeof kVietate / sizeof kVietate[0]; k++)
      if (temaFinisceCon(s, kVietate[k])) { s.e = TEMA_VIETATO; break; }
  }
  return s.e;
}

// Fine del file: un file senza byte e vuoto (stesso trattamento del file mancante: aspetto di fabbrica).
inline TemaEsito temaScanFine(TemaScan& s) {
  if (s.e == TEMA_OK && s.tot == 0) s.e = TEMA_VUOTO;
  return s.e;
}

// Parola breve per il registro e per l'intestazione X-VOS-Tema.
inline const char* temaEsitoNome(TemaEsito e) {
  switch (e) {
    case TEMA_OK: return "ok";
    case TEMA_VUOTO: return "fabbrica";
    case TEMA_GRANDE: return "troppo-grande";
    case TEMA_NUL: return "byte-nullo";
    default: return "vietato";
  }
}
