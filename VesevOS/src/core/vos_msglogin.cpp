// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_msglogin.cpp
// Controllo e codifica del messaggio della pagina di accesso (vedi vos_msglogin.h).
#include "vos_msglogin.h"
#include "vos_i18n.h"
#include <string.h>

static bool ammesso(char c) {
  if (c == '\n') return true;
  return c >= 32 && c <= 126 && c != '\\';
}

bool msgLoginControlla(std::string_view in, char* out, size_t cap, ErrBuf& err) {
  out[0] = 0;
  size_t n = 0;
  for (char c : in) {
    if (c == '\r') continue;                                   // gli a capo di Windows diventano a capo semplici
    if (!ammesso(c)) {
      err = tr("Messaggio: solo lettere, numeri e segni semplici (senza accenti e senza barra rovesciata)");
      return false;
    }
    if (n >= MSGLOGIN_MAX || n + 1 >= cap) {
      err = tr("Messaggio troppo lungo (massimo 200 caratteri)");
      return false;
    }
    out[n++] = c;
  }
  while (n > 0 && (out[n - 1] == ' ' || out[n - 1] == '\n')) n--;      // spazi e a capo in fondo
  size_t ini = 0;
  while (ini < n && (out[ini] == ' ' || out[ini] == '\n')) ini++;      // e all'inizio
  if (ini) memmove(out, out + ini, n - ini);
  out[n - ini] = 0;
  return true;
}

void msgLoginCodifica(const char* in, char* out, size_t cap) {
  size_t n = 0;
  for (const char* p = in; *p; p++) {
    if (*p == '\n') {
      if (n + 2 >= cap) break;
      out[n++] = '\\';
      out[n++] = 'n';
    } else {
      if (n + 1 >= cap) break;
      out[n++] = *p;
    }
  }
  out[n] = 0;
}

void msgLoginDecodifica(std::string_view in, char* out, size_t cap) {
  size_t n = 0;
  for (size_t i = 0; i < in.size() && n + 1 < cap; i++) {
    if (in[i] == '\\' && i + 1 < in.size() && in[i + 1] == 'n') {
      out[n++] = '\n';
      i++;
    } else {
      out[n++] = in[i];
    }
  }
  out[n] = 0;
}
