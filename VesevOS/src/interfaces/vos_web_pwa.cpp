// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_web_pwa.cpp
// App web: il manifest (GET /manifest.webmanifest, con il nome della scheda) e le tre icone PNG (/apple-touch-icon.png, /icon-192.png, /icon-512.png),
// tutte pubbliche (le chiede il browser prima dell'accesso). Le icone sono dentro il firmware (interfaces/vos_icone_data.h, generato da tools/mkicone.py).
// Nessun service worker: la scheda e gia il server e il browser lo vuole solo con HTTPS fidato.
#include "vos_web_int.h"
#include "../core/vos_manifest_rule.h"
#include "vos_icone_data.h"

namespace webx {

esp_err_t sendManifest(Res* s) {
  char buf[640];
  size_t n = manifestJson(buf, sizeof buf, cfg.hostname.c_str());
  if (n == 0) return notFound(s);
  s->setCode(200);
  s->setContentType("application/manifest+json");
  s->addHeader("Cache-Control", "no-cache");
  s->setContent((const uint8_t*)buf, n);
  return s->send();
}

// which: 180, 192 o 512 (lato dell'icona).
esp_err_t sendIcona(Res* s, int which) {
  const uint8_t* dati = which == 180 ? kIcona180 : which == 192 ? kIcona192 : which == 512 ? kIcona512 : nullptr;
  size_t len = which == 180 ? kIcona180Len : which == 192 ? kIcona192Len : which == 512 ? kIcona512Len : 0;
  if (!dati) return notFound(s);
  s->setCode(200);
  s->setContentType("image/png");
  s->addHeader("Cache-Control", "public, max-age=86400");
  s->setContent(dati, len);
  return s->send();
}

}  // namespace webx
