// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_drv_mdns.cpp
#include "vos_drv_mdns.h"
#include <stdio.h>
#include <string.h>
#include <mdns.h>

bool drvMdnsBegin(const char* hostname) {
  if (mdns_init() != ESP_OK) return false;
  return mdns_hostname_set(hostname) == ESP_OK;
}

bool drvMdnsAddService(const char* type, const char* proto, uint16_t port) {
  char t[24], p[8];
  snprintf(t, sizeof t, "_%s", type);          // il componente vuole il trattino basso davanti
  snprintf(p, sizeof p, "_%s", proto);
  return mdns_service_add(nullptr, t, p, port, nullptr, 0) == ESP_OK;
}

void drvMdnsEnd() { mdns_free(); }

bool drvMdnsResolve(const char* name, uint32_t timeoutMs, uint32_t& ip) {
  char host[64];
  snprintf(host, sizeof host, "%s", name ? name : "");
  size_t n = strlen(host);
  if (n > 6 && strcmp(host + n - 6, ".local") == 0) host[n - 6] = 0;     // la funzione vuole il nome senza ".local"
  if (!host[0]) return false;
  esp_ip4_addr_t addr;
  if (mdns_query_a(host, timeoutMs, &addr) != ESP_OK || addr.addr == 0) return false;
  ip = ((addr.addr & 0xFF) << 24) | ((addr.addr & 0xFF00) << 8) | ((addr.addr >> 8) & 0xFF00) | (addr.addr >> 24);   // da rete (byte 1 per primo) a "primo numero in alto"
  return true;
}
