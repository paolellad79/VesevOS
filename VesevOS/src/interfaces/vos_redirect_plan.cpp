// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_redirect_plan.cpp
// Parte PURA del redirect da HTTP a HTTPS: decide cosa rispondere e quale server tiene la porta HTTP. Non dipende dal server web
// (e usata sia dal server completo, vos_web_redirect.cpp, sia da quello minimo, vos_http_redirect_srv.cpp).
// La destinazione nasce SOLO dalle impostazioni della scheda (nome o IP) e dalla porta HTTPS, MAI dall'intestazione Host.
#include "vos_redirect_plan.h"
#include "../core/vos_config.h"
#include "../core/vos_hooks.h"
#include "../core/vos_i18n.h"
#include "../net/vos_net.h"
#include <stdio.h>

namespace webx {

// Nome con cui raggiungere la scheda: nome.dominio se il dominio e impostato, altrimenti l'indirizzo IP in uso.
static void targetHost(char* out, size_t n) {
  if (cfg.domain.length()) snprintf(out, n, "%s.%s", cfg.hostname.c_str(), cfg.domain.c_str());
  else netIpTo(out, n);
}

// Il percorso della richiesta e fatto solo di caratteri ammessi in un indirizzo (niente spazi o controlli)?
static bool cleanUri(const char* p) {
  for (; *p; p++) if ((unsigned char)*p <= ' ' || (unsigned char)*p >= 127) return false;
  return true;
}

int redirectPlan(bool methodOk, const char* path, const char* query, char* url, size_t cap, const char** text) {
  if (!methodOk) { *text = tr("Usa HTTPS"); return 405; }
  if (!*path) path = "/";
  if (*path != '/' || !cleanUri(path) || !cleanUri(query)) { *text = tr("Richiesta non valida"); return 400; }
  char host[96], port[8] = "";      // nome (32) + "." + dominio (60) + zero: prima 80 poteva tagliarlo
  targetHost(host, sizeof host);
  if (cfg.httpsPort != 443) snprintf(port, sizeof port, ":%u", (unsigned)cfg.httpsPort);
  int n = snprintf(url, cap, "https://%s%s%s%s%s", host, port, path, *query ? "?" : "", query);
  if (n < 0 || n >= (int)cap) { *text = tr("Indirizzo troppo lungo"); return 414; }
  if (!cleanUri(url)) { *text = tr("Richiesta non valida"); return 400; }    // difesa in piu: nome o dominio con caratteri strani
  *text = "";
  return 307;
}

// Il minimo solo se l'hotspot non puo accendersi: l'hotspot parte con il servizio acceso, senza Wi-Fi di casa configurata (vedi apAllowed in
// net/vos_net.cpp) o se e gia acceso. Ai suoi client servono portale e pagina, che il minimo non ha.
bool httpMinimalWanted() {
  bool apPossible = cfg.apOn || !cfg.staEnabled || !cfg.staSsid.length() || netState() == NET_AP;
  return cfg.httpRedirect && cfg.httpOn && cfg.https && hookWebHttpsUp() && !apPossible;
}

}  // namespace webx
