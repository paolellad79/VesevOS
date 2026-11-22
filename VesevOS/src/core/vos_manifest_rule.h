// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_manifest_rule.h
// Il manifest dell'app web (GET /manifest.webmanifest): JSON con il nome della scheda, cosi ogni scheda ha la sua icona e il suo nome sulla schermata
// Home. Senza dipendenze: provato sul computer (tools/hosttest/prova_pwa.sh). Il nome host puo avere solo lettere, numeri e trattino (regola
// dell'impostazione); qui si tengono comunque solo quelli, per non rompere mai il JSON. Chi lo usa: interfaces/vos_web_pwa.cpp.
#pragma once
#include <stddef.h>
#include <stdio.h>

// Scrive il manifest in out (cap byte). Ritorna i byte scritti, 0 se non ci sta.
inline size_t manifestJson(char* out, size_t cap, const char* host) {
  char nome[40];
  size_t n = 0;
  for (const char* p = host ? host : ""; *p && n < sizeof nome - 1; p++) {
    char c = *p;
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-') nome[n++] = c;
  }
  if (n == 0) {
    const char* base = "vesevos";
    for (; base[n]; n++) nome[n] = base[n];
  }
  nome[n] = 0;
  int w = snprintf(out, cap,
    "{\"name\":\"VesevOS %s\",\"short_name\":\"%s\",\"description\":\"VesevOS\",\"id\":\"/\",\"start_url\":\"/\",\"scope\":\"/\","
    "\"display\":\"standalone\",\"background_color\":\"#0b1220\",\"theme_color\":\"#0b1220\","
    "\"icons\":[{\"src\":\"/icon-192.png\",\"sizes\":\"192x192\",\"type\":\"image/png\",\"purpose\":\"any\"},"
    "{\"src\":\"/icon-512.png\",\"sizes\":\"512x512\",\"type\":\"image/png\",\"purpose\":\"any\"}]}",
    nome, nome);
  return (w > 0 && (size_t)w < cap) ? (size_t)w : 0;
}
