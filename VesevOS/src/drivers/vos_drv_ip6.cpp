// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_drv_ip6.cpp
// Come funziona: tutto passa dall'interfaccia client "WIFI_STA_DEF" di esp_netif. Con IPv6 acceso, in IDF 5.5 la Wi-Fi client NON crea da sola
// l'indirizzo locale (lo fa solo il modo NAN): lo creiamo noi con esp_netif_create_ip6_linklocal, e da quel momento il router puo dare
// gli indirizzi globali (SLAAC). Le funzioni di esp_netif girano nel task di lwIP: qui non servono blocchi nostri.
#include "vos_drv_ip6.h"
#include <string.h>
#include <stdio.h>
#include <esp_netif.h>
#include <lwip/sockets.h>

// Valido anche senza IPv6: serve a chi legge gli indirizzi dei client.
bool drvIp6Finto(uint32_t ip) {
  return (ip & 0xF0000000UL) == 0xF0000000UL;
}

#if CONFIG_LWIP_IPV6
static uint32_t finto64(const uint8_t b[16]) {     // FNV-1a sui primi 8 byte (la rete /64), 28 bit
  uint32_t h = 2166136261UL;
  for (int i = 0; i < 8; i++) {
    h ^= b[i];
    h *= 16777619UL;
  }
  return 0xF0000000UL | (h & 0x0FFFFFFFUL);
}

uint32_t drvSockPeer(int sock) {
  struct sockaddr_storage a;
  socklen_t n = sizeof a;
  if (getpeername(sock, (struct sockaddr*)&a, &n) < 0) return 0;
  if (a.ss_family == AF_INET) return ntohl(((struct sockaddr_in*)&a)->sin_addr.s_addr);
  if (a.ss_family != AF_INET6) return 0;
  const uint8_t* b = ((struct sockaddr_in6*)&a)->sin6_addr.s6_addr;
  bool mappato = b[10] == 0xff && b[11] == 0xff;
  for (int i = 0; i < 10 && mappato; i++) mappato = b[i] == 0;
  if (mappato) return ((uint32_t)b[12] << 24) | ((uint32_t)b[13] << 16) | ((uint32_t)b[14] << 8) | b[15];
  return finto64(b);
}
#else
uint32_t drvSockPeer(int sock) {
  struct sockaddr_in a;
  socklen_t n = sizeof a;
  if (getpeername(sock, (struct sockaddr*)&a, &n) < 0) return 0;
  return ntohl(a.sin_addr.s_addr);
}
#endif

#if CONFIG_LWIP_IPV6

namespace {

const int kLwipMax = CONFIG_LWIP_IPV6_NUM_ADDRESSES;

esp_netif_t* interfacciaClient() {
  return esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
}

// In lwIP l'indirizzo e in memoria nell'ordine della rete: i 16 byte sono gia quelli giusti.
void daByte(const esp_ip6_addr_t& a, uint8_t b[16]) {
  memcpy(b, a.addr, 16);
}

DrvIp6Tipo tipoDi(const uint8_t b[16]) {
  if (b[0] == 0xfe && (b[1] & 0xc0) == 0x80) return DI6T_LOCALE;
  if ((b[0] & 0xe0) == 0x20) return DI6T_GLOBALE;
  if ((b[0] & 0xfe) == 0xfc) return DI6T_PRIVATO;
  return DI6T_ALTRO;
}

// Testo in forma breve: la serie piu lunga di gruppi a zero (almeno 2) diventa "::".
void aTesto(const uint8_t b[16], char* out, size_t n) {
  uint16_t g[8];
  for (int i = 0; i < 8; i++) g[i] = (uint16_t)((b[2 * i] << 8) | b[2 * i + 1]);
  int migliorePos = -1;
  int miglioreLun = 1;
  for (int i = 0; i < 8; i++) {
    int lun = 0;
    while (i + lun < 8 && g[i + lun] == 0) lun++;
    if (lun > miglioreLun) {
      miglioreLun = lun;
      migliorePos = i;
    }
    if (lun > 1) i += lun - 1;
  }
  size_t k = 0;
  out[0] = 0;
  for (int i = 0; i < 8; i++) {
    if (i == migliorePos) {
      if (k + 2 < n) {
        out[k++] = ':';
        out[k++] = ':';
        out[k] = 0;
      }
      i += miglioreLun - 1;
      continue;
    }
    if (k > 0 && out[k - 1] != ':' && k + 1 < n) {
      out[k++] = ':';
      out[k] = 0;
    }
    int w = snprintf(out + k, n - k, "%x", (unsigned)g[i]);
    if (w < 0 || (size_t)w >= n - k) break;
    k += (size_t)w;
  }
}

bool daTesto(const char* in, esp_ip6_addr_t* a) {
  if (!in || !in[0] || strlen(in) >= 46) return false;
  memset(a, 0, sizeof *a);
  return esp_netif_str_to_ip6(in, a) == ESP_OK;
}

bool esiste(const uint8_t cerca[16]) {
  esp_netif_t* nif = interfacciaClient();
  if (!nif) return false;
  esp_ip6_addr_t tutti[CONFIG_LWIP_IPV6_NUM_ADDRESSES];
  int n = esp_netif_get_all_ip6(nif, tutti);
  for (int i = 0; i < n && i < kLwipMax; i++) {
    uint8_t b[16];
    daByte(tutti[i], b);
    if (memcmp(b, cerca, 16) == 0) return true;
  }
  return false;
}

}  // namespace

void drvIp6Normale(const uint8_t b[16], char* out, size_t n) {
  aTesto(b, out, n);
}

bool drvIp6Presente() {
  return true;
}

bool drvIp6CreaLocale() {
  esp_netif_t* nif = interfacciaClient();
  return nif && esp_netif_create_ip6_linklocal(nif) == ESP_OK;
}

int drvIp6Elenca(DrvIp6Voce* out, int max) {
  esp_netif_t* nif = interfacciaClient();
  if (!nif || !out || max <= 0) return 0;
  esp_ip6_addr_t tutti[CONFIG_LWIP_IPV6_NUM_ADDRESSES];
  esp_ip6_addr_t scelti[CONFIG_LWIP_IPV6_NUM_ADDRESSES];
  int n = esp_netif_get_all_ip6(nif, tutti);
  int p = esp_netif_get_all_preferred_ip6(nif, scelti);
  int dati = 0;
  for (int i = 0; i < n && i < kLwipMax && dati < max && dati < DRV_IP6_MAX; i++) {
    uint8_t b[16];
    daByte(tutti[i], b);
    aTesto(b, out[dati].testo, sizeof out[dati].testo);
    out[dati].tipo = tipoDi(b);
    out[dati].preferito = false;
    for (int j = 0; j < p && j < kLwipMax; j++) {
      uint8_t c[16];
      daByte(scelti[j], c);
      if (memcmp(b, c, 16) == 0) out[dati].preferito = true;
    }
    dati++;
  }
  return dati;
}

DrvIp6Esito drvIp6Controlla(const char* in, char* norm, size_t n) {
  esp_ip6_addr_t a;
  if (!daTesto(in, &a)) return DI6E_NON_VALIDO;
  uint8_t b[16];
  daByte(a, b);
  DrvIp6Tipo t = tipoDi(b);
  if (t != DI6T_GLOBALE && t != DI6T_PRIVATO) return DI6E_NON_AMMESSO;
  if (norm && n) aTesto(b, norm, n);
  return DI6E_OK;
}

bool drvIp6Aggiungi(const char* norm) {
  esp_netif_t* nif = interfacciaClient();
  esp_ip6_addr_t a;
  if (!nif || !daTesto(norm, &a)) return false;
  uint8_t b[16];
  daByte(a, b);
  if (esiste(b)) return true;
  return esp_netif_add_ip6_address(nif, a, true) == ESP_OK;
}

bool drvIp6Togli(const char* norm) {
  esp_netif_t* nif = interfacciaClient();
  esp_ip6_addr_t a;
  if (!nif || !daTesto(norm, &a)) return false;
  return esp_netif_remove_ip6_address(nif, &a) == ESP_OK;
}

void drvIp6TogliTutti() {
  esp_netif_t* nif = interfacciaClient();
  if (!nif) return;
  esp_ip6_addr_t tutti[CONFIG_LWIP_IPV6_NUM_ADDRESSES];
  int n = esp_netif_get_all_ip6(nif, tutti);
  for (int i = 0; i < n && i < kLwipMax; i++) esp_netif_remove_ip6_address(nif, &tutti[i]);
}

#else  // senza IPv6 (recovery)

void        drvIp6Normale(const uint8_t*, char* out, size_t) { out[0] = 0; }
bool        drvIp6Presente() { return false; }
bool        drvIp6CreaLocale() { return false; }
int         drvIp6Elenca(DrvIp6Voce*, int) { return 0; }
DrvIp6Esito drvIp6Controlla(const char*, char*, size_t) { return DI6E_NON_VALIDO; }
bool        drvIp6Aggiungi(const char*) { return false; }
bool        drvIp6Togli(const char*) { return false; }
void        drvIp6TogliTutti() {}

#endif
