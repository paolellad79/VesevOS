// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_drv_ip6_rotta.cpp
// Come funziona: lo stato del gateway fisso lo scrive solo il task di lwIP (con tcpip_callback), cosi i ganci, che girano nello stesso
// task, lo leggono senza blocchi. Il DNS si scrive con dns_setserver, sempre dal task di lwIP, nello slot di riserva.
#include "vos_drv_ip6_rotta.h"
#include <string.h>
#include <esp_netif.h>

#if CONFIG_LWIP_IPV6

#include <lwip/netif.h>
#include <lwip/ip6_addr.h>
#include <lwip/dns.h>
#include <lwip/tcpip.h>

namespace {

struct Richiesta {
  bool    attivo;
  uint8_t byte[16];
  int     indice;      // indice lwIP dell'interfaccia client
};

Richiesta   g_gwRichiesta;
Richiesta   g_dnsRichiesta;
ip6_addr_t  g_gw;            // scritti e letti solo nel task di lwIP
bool        g_gwAttivo = false;
int         g_gwIndice = 0;

bool daTesto(const char* in, esp_ip6_addr_t* a) {
  if (!in || !in[0] || strlen(in) >= 46) return false;
  memset(a, 0, sizeof *a);
  return esp_netif_str_to_ip6(in, a) == ESP_OK;
}

int indiceClient() {
  esp_netif_t* nif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
  return nif ? esp_netif_get_netif_impl_index(nif) : 0;
}

bool linkLocale(const uint8_t b[16]) {
  return b[0] == 0xfe && (b[1] & 0xc0) == 0x80;
}

bool stessaRete64(const uint8_t a[16], const uint8_t b[16]) {
  return memcmp(a, b, 8) == 0;
}

// Eseguiti nel task di lwIP.
void applicaGateway(void*) {
  g_gwAttivo = false;
  if (!g_gwRichiesta.attivo) return;
  struct netif* nif = netif_get_by_index((uint8_t)g_gwRichiesta.indice);
  if (!nif) return;
  memset(&g_gw, 0, sizeof g_gw);
  memcpy(g_gw.addr, g_gwRichiesta.byte, 16);
  ip6_addr_assign_zone(&g_gw, IP6_UNICAST, nif);   // un indirizzo locale vale solo su una interfaccia
  g_gwIndice = g_gwRichiesta.indice;
  g_gwAttivo = true;
}

void applicaDns(void*) {
  if (!g_dnsRichiesta.attivo) {
    dns_setserver(DNS_FALLBACK_SERVER_INDEX, NULL);
    return;
  }
  ip_addr_t a;
  memset(&a, 0, sizeof a);
  IP_SET_TYPE_VAL(a, IPADDR_TYPE_V6);
  memcpy(ip_2_ip6(&a)->addr, g_dnsRichiesta.byte, 16);
  dns_setserver(DNS_FALLBACK_SERVER_INDEX, &a);
}

}  // namespace

// Ganci di lwIP (definizioni "forti": prendono il posto di quelle deboli di IDF, vedi lwip_default_hooks.c).
extern "C" {

const ip6_addr_t* lwip_hook_nd6_get_gw(struct netif* netif, const ip6_addr_t* dest) {
  (void)dest;
  if (!g_gwAttivo || netif_get_index(netif) != g_gwIndice) return NULL;
  return &g_gw;
}

struct netif* lwip_hook_ip6_route(const ip6_addr_t* src, const ip6_addr_t* dest) {
  (void)src;
  (void)dest;
  if (!g_gwAttivo) return NULL;
  struct netif* nif = netif_get_by_index((uint8_t)g_gwIndice);
  if (nif && netif_is_up(nif) && netif_is_link_up(nif)) return nif;
  return NULL;
}

}  // extern "C"

DrvIp6Esito drvIp6ControllaGw(const char* in, const char* fisso, char* norm, size_t n) {
  esp_ip6_addr_t a;
  if (!daTesto(in, &a)) return DI6E_NON_VALIDO;
  uint8_t b[16];
  memcpy(b, a.addr, 16);
  bool raggiungibile = linkLocale(b);
  esp_ip6_addr_t altro;
  if (!raggiungibile && fisso && daTesto(fisso, &altro)) {
    raggiungibile = stessaRete64(b, (const uint8_t*)altro.addr);
  }
  DrvIp6Voce voci[DRV_IP6_MAX];
  int quante = raggiungibile ? 0 : drvIp6Elenca(voci, DRV_IP6_MAX);
  for (int i = 0; i < quante && !raggiungibile; i++) {
    if (daTesto(voci[i].testo, &altro)) raggiungibile = stessaRete64(b, (const uint8_t*)altro.addr);
  }
  if (!raggiungibile) return DI6E_NON_AMMESSO;
  if (norm && n) drvIp6Normale(b, norm, n);
  return DI6E_OK;
}

DrvIp6Esito drvIp6ControllaDns(const char* in, char* norm, size_t n) {
  return drvIp6Controlla(in, norm, n);   // stessa regola dell'indirizzo fisso: globale o privato
}

bool drvIp6ImpostaGateway(const char* norm) {
  esp_ip6_addr_t a;
  g_gwRichiesta.attivo = norm && norm[0];
  if (g_gwRichiesta.attivo) {
    if (!daTesto(norm, &a)) return false;
    memcpy(g_gwRichiesta.byte, a.addr, 16);
  }
  g_gwRichiesta.indice = indiceClient();
  return tcpip_callback(applicaGateway, NULL) == ERR_OK;
}

bool drvIp6ImpostaDns(const char* norm) {
  esp_ip6_addr_t a;
  g_dnsRichiesta.attivo = norm && norm[0];
  if (g_dnsRichiesta.attivo) {
    if (!daTesto(norm, &a)) return false;
    memcpy(g_dnsRichiesta.byte, a.addr, 16);
  }
  return tcpip_callback(applicaDns, NULL) == ERR_OK;
}

#else  // senza IPv6 (recovery)

DrvIp6Esito drvIp6ControllaGw(const char*, const char*, char*, size_t) { return DI6E_NON_VALIDO; }
DrvIp6Esito drvIp6ControllaDns(const char*, char*, size_t) { return DI6E_NON_VALIDO; }
bool drvIp6ImpostaGateway(const char*) { return false; }
bool drvIp6ImpostaDns(const char*) { return false; }

#endif
