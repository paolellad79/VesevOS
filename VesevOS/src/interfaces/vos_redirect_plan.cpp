// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_redirect_plan.cpp
// Parte PURA del redirect da HTTP a HTTPS: decide cosa rispondere e quale server tiene la porta HTTP. Non dipende dal server web
// (e usata sia dal server completo, vos_web_redirect.cpp, sia da quello minimo, vos_http_redirect_srv.cpp).
// La destinazione nasce SOLO dalle impostazioni della scheda (nome o IP) e dalla porta HTTPS, MAI dall'intestazione Host.
#include "vos_redirect_plan.h"
#include "../core/vos_config.h"
#include "../core/vos_i18n.h"
#include "../drivers/vos_drv_dhcp.h"
#include "../net/vos_net.h"
#include <stdio.h>

namespace webx {

// Nome con cui raggiungere la scheda, nell'ordine: dominio impostato, dominio dato dal router (DHCP),
// nome.local se l'annuncio del nome e acceso, altrimenti l'indirizzo IP in uso.
static void targetHost(char* out, size_t n) {
  char routerDomain[64];
  if (cfg.domain.length()) {
    snprintf(out, n, "%.32s.%.60s", cfg.hostname.c_str(), cfg.domain.c_str());   // limiti del nome (32) e del dominio (60)
  } else if (drvDhcpDomain(routerDomain, sizeof routerDomain)) {
    snprintf(out, n, "%.32s.%.60s", cfg.hostname.c_str(), routerDomain);
  } else if (cfg.mdnsOn) {
    snprintf(out, n, "%.32s.local", cfg.hostname.c_str());
  } else {
    netIpTo(out, n);
  }
}

// Il percorso della richiesta e fatto solo di caratteri ammessi in un indirizzo (niente spazi o controlli)?
static bool cleanUri(const char* p) {
  for (; *p; p++) if ((unsigned char)*p <= ' ' || (unsigned char)*p >= 127) return false;
  return true;
}

int redirectPlan(uint32_t ip, bool methodOk, const char* path, const char* query, char* url, size_t cap, const char** text) {
  if (!methodOk) { *text = tr("Usa HTTPS"); return 405; }
  bool suHotspot = netOnAp(ip);
  if (suHotspot) { path = "/"; query = ""; }
  if (!*path) path = "/";
  if (*path != '/' || !cleanUri(path) || !cleanUri(query)) { *text = tr("Richiesta non valida"); return 400; }
  char host[96], port[8] = "";      // nome (32) + "." + dominio (60) + zero: prima 80 poteva tagliarlo
  if (suHotspot) snprintf(host, sizeof host, "192.168.4.1");
  else targetHost(host, sizeof host);
  if (cfg.httpsPort != 443) snprintf(port, sizeof port, ":%u", (unsigned)cfg.httpsPort);
  int n = snprintf(url, cap, "https://%s%s%s%s%s", host, port, path, *query ? "?" : "", query);
  if (n < 0 || n >= (int)cap) { *text = tr("Indirizzo troppo lungo"); return 414; }
  if (!cleanUri(url)) { *text = tr("Richiesta non valida"); return 400; }    // difesa in piu: nome o dominio con caratteri strani
  *text = "";
  return 307;
}

}  // namespace webx
