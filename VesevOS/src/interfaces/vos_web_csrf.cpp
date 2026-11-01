// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_web_csrf.cpp (vedi vos_web_csrf.h)
#include "vos_web_csrf.h"
#include <string.h>
#include <strings.h>

namespace webx {

static bool has(const char* s) { return s && *s; }

bool csrfCrossSite(const char* secFetchSite, const char* origin, const char* host) {
  if (has(secFetchSite)) return strcasecmp(secFetchSite, "same-origin") != 0 && strcasecmp(secFetchSite, "none") != 0;
  if (!has(origin)) return false;                                   // niente da controllare: programma, non browser
  const char* a = strstr(origin, "://");
  if (!a || !has(host)) return true;                                // "null", schemi strani o niente Host: si rifiuta
  a += 3;
  size_t n = strlen(a);
  while (n && a[n - 1] == '/') n--;                                 // Origin non ha il percorso, ma uno "/" finale non cambia l'indirizzo
  return n != strlen(host) || strncasecmp(a, host, n) != 0;
}

}  // namespace webx
