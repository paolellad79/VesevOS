// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_vdesc.h
// Aiuti per leggere i file descrittivi a righe (.vap delle app, .vdp delle schede modulo) senza la classe di testo di Arduino: lavorano su viste e buffer fissi.
// heapAddf scrive un testo formattato in un HeapBuf (per i JSON e i testi della shell); il resto sono piccole funzioni sulle viste.
#pragma once
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string_view>
#include "vos_heapbuf.h"
#include "vos_parse.h"

// Come printf, ma in coda a un HeapBuf. false se manca la memoria o si supera il tetto (il testo non entra per intero e non si scrive nulla).
inline bool heapAddf(HeapBuf& out, const char* fmt, ...) __attribute__((format(printf, 2, 3)));
inline bool heapAddf(HeapBuf& out, const char* fmt, ...) {
  char small[160];
  va_list ap; va_start(ap, fmt);
  int n = vsnprintf(small, sizeof small, fmt, ap);
  va_end(ap);
  if (n < 0) return false;
  if ((size_t)n < sizeof small) return out.add(small, (size_t)n);
  char* big = (char*)malloc((size_t)n + 1);                   // testo piu lungo del buffer piccolo: raro
  if (!big) return false;
  va_start(ap, fmt);
  vsnprintf(big, (size_t)n + 1, fmt, ap);
  va_end(ap);
  bool ok = out.add(big, (size_t)n);
  free(big);
  return ok;
}

// Righe di un testo, come il vecchio ciclo con indexOf('\n'): l'ultima riga (anche vuota) c'e sempre; il '\r' finale si toglie.
class LineIter {
 public:
  explicit LineIter(std::string_view text) : t_(text) {}
  bool next(std::string_view& raw) {
    if (pos_ > t_.size()) return false;
    size_t e = t_.find('\n', pos_);
    if (e == std::string_view::npos) e = t_.size();
    raw = t_.substr(pos_, e - pos_);
    pos_ = e + 1;
    line_++;
    if (!raw.empty() && raw.back() == '\r') raw.remove_suffix(1);
    return true;
  }
  int line() const { return line_; }
 private:
  std::string_view t_;
  size_t pos_ = 0;
  int line_ = 0;
};

inline char vdLower(char c) { return (c >= 'A' && c <= 'Z') ? (char)(c + 32) : c; }
inline char vdUpper(char c) { return (c >= 'a' && c <= 'z') ? (char)(c - 32) : c; }
inline bool viewEqI(std::string_view v, const char* lit) {        // uguale a lit senza badare alle maiuscole
  size_t n = strlen(lit);
  if (v.size() != n) return false;
  for (size_t i = 0; i < n; i++) if (vdLower(v[i]) != vdLower(lit[i])) return false;
  return true;
}
// Copia v in dst (cap byte, con lo zero). false se non ci sta (dst resta vuoto).
inline bool viewCopy(char* dst, size_t cap, std::string_view v) {
  if (v.size() >= cap) { dst[0] = 0; return false; }
  memcpy(dst, v.data(), v.size());
  dst[v.size()] = 0;
  return true;
}
// Come viewCopy, ma in minuscolo / in maiuscolo.
inline bool viewCopyLower(char* dst, size_t cap, std::string_view v) {
  if (!viewCopy(dst, cap, v)) return false;
  for (size_t i = 0; i < v.size(); i++) dst[i] = vdLower(dst[i]);
  return true;
}
inline bool viewCopyUpper(char* dst, size_t cap, std::string_view v) {
  if (!viewCopy(dst, cap, v)) return false;
  for (size_t i = 0; i < v.size(); i++) dst[i] = vdUpper(dst[i]);
  return true;
}
// Aggiunge v in coda a dst (cap byte) se ci sta tutto. false se non ci sta (non si scrive nulla).
inline bool bufAppend(char* dst, size_t cap, size_t& len, std::string_view v) {
  if (len + v.size() >= cap) return false;
  memcpy(dst + len, v.data(), v.size());
  len += v.size();
  dst[len] = 0;
  return true;
}
