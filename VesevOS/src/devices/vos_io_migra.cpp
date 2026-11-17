// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_io_migra.cpp
// Migrazione dei nomi salvati in io.cfg (decisione dell'utente del 13/11/2026): i LED di bordo hanno nomi nuovi (led_rgb1, led1...),
// quindi una periferica a mano che si chiama come uno di loro (es. "led1" su C6/C3) prende il primo numero libero della sua classe
// e l'utente riceve un avviso. E' il punto UNICO di questa migrazione.
#include "vos_io_int.h"
#include "vos_dev.h"
#include "vos_led_nomi.h"
#include "../core/vos_i18n.h"
#include "../core/vos_log.h"
#include <stdio.h>
#include <string.h>

static bool g_fatta = false;

int ioMigraNumeroIniziale(const char* prefisso) {
  if (strcmp(prefisso, "led") == 0) return ledNumeroIniziale("led");
  if (strcmp(prefisso, "led_rgb") == 0) return ledNumeroIniziale("led_rgb");
  return 1;
}

bool ioMigraNomeBordo(const char* nm, const char* prefisso, int (*occupato)(const char*), char* out) {
  if (!devFind(nm)) return false;                                         // nome libero: niente da fare
  for (int k = ioMigraNumeroIniziale(prefisso); k < 100; k++) {
    char t[16];
    snprintf(t, sizeof t, "%s%d", prefisso, k);
    if (occupato(t) < 0 && !ioNameReserved(t)) {
      vlog("%s", trf("IO: il nome %s ora e di una periferica della scheda: la tua periferica si chiama %s", nm, t).c_str());
      strncpy(out, t, 15);
      out[15] = 0;
      g_fatta = true;
      return true;
    }
  }
  return false;                                                           // nessun numero libero: la riga sara scartata come prima
}

bool ioMigraFatta() {
  bool f = g_fatta;
  g_fatta = false;
  return f;
}
