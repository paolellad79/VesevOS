// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_sd_buf.h
// Aiuti per scrivere testo in un buffer di char con lunghezza (niente String di Arduino).
// Usati da vos_sd.cpp e vos_sd_act.cpp. Non scrive mai oltre la fine: "ov" dice se qualcosa non c'e stato.
#pragma once
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

struct Buf {
  char* b;
  size_t n;
  size_t len;
  bool ov;

  Buf(char* p, size_t sz) : b(p), n(sz), len(0), ov(false) {
    if (n) {
      b[0] = 0;
    } else {
      ov = true;
    }
  }

  void ch(char c) {
    if (len + 1 < n) {
      b[len++] = c;
      b[len] = 0;
    } else {
      ov = true;
    }
  }

  void put(const char* s) {
    for (; *s; s++) {
      ch(*s);
    }
  }

  void fmt(const char* f, ...) __attribute__((format(printf, 2, 3))) {
    if (len + 1 >= n) {
      ov = true;
      return;
    }

    va_list ap;
    va_start(ap, f);
    int written = vsnprintf(b + len, n - len, f, ap);
    va_end(ap);

    if (written < 0 || (size_t)written >= n - len) {
      ov = true;
      len = n - 1;
    } else {
      len += (size_t)written;
    }
  }

  // Testo tra virgolette JSON (solo ASCII: il resto diventa un punto interrogativo).
  void esc(const char* s) {
    for (; *s; s++) {
      unsigned char c = (unsigned char)*s;

      if (c == '"' || c == '\\') {
        ch('\\');
        ch((char)c);
      } else if (c == '\n') {
        put("\\n");
      } else if (c < 32 || c > 126) {
        ch('?');
      } else {
        ch((char)c);
      }
    }
  }
};

inline void sdSetErr(char* err, size_t en, const char* msg) {
  if (err && en) {
    snprintf(err, en, "%s", msg);
  }
}

// Una riga ["etichetta","valore"] dell'elenco "rows".
inline void sdRow(Buf& j, bool& first, const char* label, const char* value) {
  if (!first) {
    j.ch(',');
  }

  first = false;
  j.put("[\"");
  j.esc(label);
  j.put("\",\"");
  j.esc(value);
  j.put("\"]");
}
