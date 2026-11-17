// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_net_ip6.cpp
// Come funziona: net6Tick confronta ogni secondo la configurazione con quello che e gia stato fatto sulla connessione in corso
// (indirizzo locale creato? indirizzo fisso applicato?) e fa solo la differenza. Cosi un cambio dalla pagina vale subito e,
// a ogni nuova connessione Wi-Fi, si riparte da zero.
#include "vos_net_ip6.h"
#include "../core/vos_config.h"
#include "../core/vos_i18n.h"
#include "../core/vos_log.h"
#include "../drivers/vos_drv_ip6.h"
#include "../drivers/vos_drv_ip6_rotta.h"
#include <stdio.h>
#include <string.h>

namespace {

bool g_localeCreato = false;                // sulla connessione in corso l'indirizzo locale e stato creato
char g_fissoApplicato[DRV_IP6_TESTO] = "";  // indirizzo fisso messo sull'interfaccia ("" = nessuno)
bool g_gwNoto = false;                      // il gateway fisso e stato mandato a lwIP (anche "nessuno") su questa connessione
char g_gwApplicato[DRV_IP6_TESTO] = "";
bool g_dnsNoto = false;
char g_dnsApplicato[DRV_IP6_TESTO] = "";

void ripartiDaZero() {
  g_localeCreato = false;
  g_fissoApplicato[0] = 0;
  g_gwNoto = false;                         // a ogni nuova connessione si manda di nuovo, anche "nessuno"
  g_dnsNoto = false;
}

void spegni() {
  drvIp6TogliTutti();
  drvIp6ImpostaGateway("");
  drvIp6ImpostaDns("");
  ripartiDaZero();
  vlog("NET: IPv6 spento");
}

void allineaIndirizzoFisso() {
  char voluto[DRV_IP6_TESTO] = "";
  if (cfg.ip6Fisso && drvIp6Controlla(cfg.ip6Ind.c_str(), voluto, sizeof voluto) != DI6E_OK) voluto[0] = 0;   // il file di configurazione non e fidato
  if (strcmp(voluto, g_fissoApplicato) == 0) return;
  if (g_fissoApplicato[0]) drvIp6Togli(g_fissoApplicato);
  if (voluto[0] && !drvIp6Aggiungi(voluto)) vlog("NET: IPv6, indirizzo fisso non applicato");
  snprintf(g_fissoApplicato, sizeof g_fissoApplicato, "%s", voluto);   // un solo tentativo per valore: niente ripetizioni ogni secondo
}

// Il gateway e i DNS a mano vengono dal file di configurazione, che non e fidato: si ricontrollano prima di applicarli.
void allineaGatewayEDns() {
  char voluto[DRV_IP6_TESTO] = "";
  if (cfg.ip6Gw.length() && drvIp6ControllaGw(cfg.ip6Gw.c_str(), cfg.ip6Fisso ? g_fissoApplicato : "", voluto, sizeof voluto) != DI6E_OK) voluto[0] = 0;
  if (!g_gwNoto || strcmp(voluto, g_gwApplicato) != 0) {
    if (drvIp6ImpostaGateway(voluto)) {
      snprintf(g_gwApplicato, sizeof g_gwApplicato, "%s", voluto);
      g_gwNoto = true;
    }
  }
  voluto[0] = 0;
  if (cfg.ip6Dns.length() && drvIp6ControllaDns(cfg.ip6Dns.c_str(), voluto, sizeof voluto) != DI6E_OK) voluto[0] = 0;
  if (!g_dnsNoto || strcmp(voluto, g_dnsApplicato) != 0) {
    if (drvIp6ImpostaDns(voluto)) {
      snprintf(g_dnsApplicato, sizeof g_dnsApplicato, "%s", voluto);
      g_dnsNoto = true;
    }
  }
}

void jTesto(HeapBuf& j, const char* chiave, const char* v) {
  j.add(",\"");
  j.add(chiave);
  j.add("\":\"");
  j.addJson(v, strlen(v));
  j.add('"');
}

void jBool(HeapBuf& j, const char* chiave, bool v) {
  j.add(",\"");
  j.add(chiave);
  j.add(v ? "\":true" : "\":false");
}

}  // namespace

void net6Tick(bool clienteCollegato) {
  if (!drvIp6Presente()) return;
  if (!clienteCollegato) {                    // senza Wi-Fi di casa l'interfaccia client non c'e
    ripartiDaZero();
    return;
  }
  if (!cfg.ip6On) {
    if (g_localeCreato) spegni();
    return;
  }
  if (!g_localeCreato) {
    if (!drvIp6CreaLocale()) return;          // la Wi-Fi non e ancora pronta: si riprova al prossimo giro
    g_localeCreato = true;
    vlog("NET: IPv6 acceso (indirizzo locale creato)");
  }
  allineaIndirizzoFisso();
  allineaGatewayEDns();
}

bool net6Json(HeapBuf& j, bool collegato) {
  bool presente = drvIp6Presente();
  j.add("{\"presente\":");
  j.add(presente ? "true" : "false");
  jBool(j, "acceso", cfg.ip6On);
  jBool(j, "fisso", cfg.ip6Fisso);
  jTesto(j, "ind", cfg.ip6Ind.c_str());
  jTesto(j, "gw", cfg.ip6Gw.c_str());
  jTesto(j, "dns", cfg.ip6Dns.c_str());
  jBool(j, "collegato", collegato);
  j.add(",\"prefisso\":64,\"elenco\":[");
  DrvIp6Voce voci[DRV_IP6_MAX];
  int n = (presente && collegato) ? drvIp6Elenca(voci, DRV_IP6_MAX) : 0;
  for (int i = 0; i < n; i++) {
    if (i) j.add(',');
    j.add("{\"a\":\"");
    j.addJson(voci[i].testo, strlen(voci[i].testo));
    j.addf("\",\"t\":%d,\"p\":%s}", (int)voci[i].tipo, voci[i].preferito ? "true" : "false");
  }
  j.add("]}");
  return !j.oom() && !j.over();
}

bool net6Salva(bool acceso, bool fisso, const char* indirizzo, const char* gateway, const char* dns, ErrBuf& err) {
  if (!drvIp6Presente()) {
    err = tr("IPv6 non e disponibile in questo firmware");
    return false;
  }
  char norma[DRV_IP6_TESTO] = "";
  if (fisso) {
    DrvIp6Esito e = drvIp6Controlla(indirizzo, norma, sizeof norma);
    if (e == DI6E_NON_VALIDO) {
      err = tr("Indirizzo IPv6 non valido");
      return false;
    }
    if (e == DI6E_NON_AMMESSO) {
      err = tr("Indirizzo IPv6 non ammesso: usa un indirizzo globale (2000::/3) o privato (fc00::/7)");
      return false;
    }
  }
  char gwNorma[DRV_IP6_TESTO] = "";
  if (gateway[0]) {
    DrvIp6Esito e = drvIp6ControllaGw(gateway, norma, gwNorma, sizeof gwNorma);
    if (e == DI6E_NON_VALIDO) {
      err = tr("Gateway IPv6 non valido");
      return false;
    }
    if (e == DI6E_NON_AMMESSO) {
      err = tr("Gateway IPv6 non raggiungibile: usa un indirizzo locale (fe80::) oppure della stessa rete /64 della scheda");
      return false;
    }
  }
  char dnsNorma[DRV_IP6_TESTO] = "";
  if (dns[0]) {
    DrvIp6Esito e = drvIp6ControllaDns(dns, dnsNorma, sizeof dnsNorma);
    if (e == DI6E_NON_VALIDO) {
      err = tr("DNS IPv6 non valido");
      return false;
    }
    if (e == DI6E_NON_AMMESSO) {
      err = tr("DNS IPv6 non ammesso: usa un indirizzo globale (2000::/3) o privato (fc00::/7)");
      return false;
    }
  }
  cfg.ip6On = acceso;
  cfg.ip6Fisso = fisso;
  if (fisso) cfg.ip6Ind = norma;
  cfg.ip6Gw = gwNorma;
  cfg.ip6Dns = dnsNorma;
  cfgSave();
  return true;
}
