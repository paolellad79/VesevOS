// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_web_pezzi.h
// Parti comuni dei caricamenti a pezzi (risorse, file, recovery). Il client e un solo codice: web/src/js_pezzi.js (funzione caricaPezzi).
// Riga dell'indirizzo: o = byte gia inviati, e = 1 se e l'ultimo pezzo. Il pezzo con o = 0 ricomincia; gli altri devono seguire quelli accettati.
// Risposta a un pezzo accettato: {"ok":true,"n":byte accettati}; fuori posto: 409 {"code":"pezzo","n":punto a cui e la scheda}.
#pragma once
#include "vos_web_int.h"

namespace webx {

const size_t PEZZO_MAX = 8192;        // byte al massimo in un pezzo (corpo della richiesta)

struct PezzoQ {
  uint32_t off;      // byte gia inviati prima di questo pezzo
  bool     ultimo;   // e l'ultimo pezzo del file
  bool     troppo;   // il corpo supera PEZZO_MAX
};

inline PezzoQ pezzoQuery(Req* r) {
  char o[12], e[4];
  qparamTo(r, "o", o, sizeof o);
  qparamTo(r, "e", e, sizeof e);
  PezzoQ q;
  q.off = (uint32_t)strtoul(o, nullptr, 10);
  q.ultimo = e[0] == '1';
  q.troppo = r->contentLength() > PEZZO_MAX;
  return q;
}

esp_err_t pezzoAccettato(Res* s, uint32_t n);        // {"ok":true,"n":n}
esp_err_t pezzoFuoriPosto(Res* s, uint32_t n);      // 409 {"code":"pezzo","n":n}

}  // namespace webx
