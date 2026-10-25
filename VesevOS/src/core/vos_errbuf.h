// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_errbuf.h
// Messaggio di errore breve in un buffer fisso (nessuna memoria dinamica, nessun Arduino). Lo riempie la funzione che fallisce,
// lo legge il chiamante (shell, pagina, regole). Se il testo e piu lungo di ERRBUF_CAP si taglia (sempre chiuso con 0).
// "vuoto" = nessun errore: serve a chi controlla un testo di rifiuto invece di un bool (cfgSvcCheck).
#pragma once
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#define ERRBUF_CAP 160

class ErrBuf {
 public:
  ErrBuf() { s_[0] = 0; }
  ErrBuf& operator=(const char* t) { copy(t); return *this; }
  void clear() { s_[0] = 0; }
  // Come printf. Per i testi tradotti: err.setf(tr("Riga %d"), n).
  void setf(const char* fmt, ...) __attribute__((format(printf, 2, 3))) {
    va_list ap; va_start(ap, fmt);
    vsnprintf(s_, sizeof(s_), fmt, ap);
    va_end(ap);
  }
  // Aggiunge in coda (per mettere un messaggio di un livello sotto dopo un'etichetta).
  void append(const char* t) {
    size_t n = strlen(s_);
    if (t && n < sizeof(s_) - 1) snprintf(s_ + n, sizeof(s_) - n, "%s", t);
  }
  const char* c_str() const { return s_; }
  bool   empty() const { return s_[0] == 0; }
  size_t length() const { return strlen(s_); }
  char*  data() { return s_; }
  size_t size() const { return sizeof(s_); }
 private:
  void copy(const char* t) {
    if (!t) { s_[0] = 0; return; }
    size_t n = strlen(t);
    if (n >= sizeof(s_)) n = sizeof(s_) - 1;
    memmove(s_, t, n);   // memmove: t puo essere dentro s_ stesso
    s_[n] = 0;
  }
  char s_[ERRBUF_CAP];
};
