// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_web_pezzi.cpp
// Risposte comuni dei caricamenti a pezzi (vedi vos_web_pezzi.h).
#include "vos_web_pezzi.h"

namespace webx {

esp_err_t pezzoAccettato(Res* s, uint32_t n) {
  char j[48];
  snprintf(j, sizeof j, "{\"ok\":true,\"n\":%u}", (unsigned)n);
  return sendJson(s, j);
}

esp_err_t pezzoFuoriPosto(Res* s, uint32_t n) {
  char j[160];
  snprintf(j, sizeof j, "{\"ok\":false,\"code\":\"pezzo\",\"n\":%u,\"err\":\"%s\"}", (unsigned)n, tr("Pezzo del file fuori posto"));
  return sendJson(s, j, 409);
}

}  // namespace webx
