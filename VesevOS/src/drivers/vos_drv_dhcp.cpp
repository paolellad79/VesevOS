// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_drv_dhcp.cpp (vedi vos_drv_dhcp.h)
// lwIP chiama lwip_dhcp_on_extra_option() (CONFIG_LWIP_HOOK_DHCP_EXTRA_OPTION_CUSTOM) per ogni opzione DHCP che non interpreta da solo.
// La funzione gira nel task di rete di lwIP: deve essere breve e non bloccare. Chi legge sta in un altro task, quindi la copia e protetta
// da un contatore (se cambia durante la lettura, si rilegge).
#include "vos_drv_dhcp.h"
#include <atomic>
#include <string.h>
#include "lwip/pbuf.h"
#include "lwip/prot/dhcp.h"

#define DHCP_OPT_DOMAIN 15
#define DOM_MAX         63     // lunghezza massima di un nome di dominio (RFC 1035) senza lo zero finale

static char                  s_dom[DOM_MAX + 1];
static std::atomic<uint32_t> s_seq{0};         // dispari = scrittura in corso

// Nome di dominio plausibile: lettere, cifre, "-" e ".", etichette non vuote (niente punto o trattino ai bordi di un'etichetta)
static bool domainOk(const char* s, size_t n) {
  if (n < 1) return false;
  bool start = true;
  for (size_t i = 0; i < n; i++) {
    char c = s[i];
    bool alnum = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9');
    if (c == '.') { if (start) return false; start = true; continue; }
    if (c == '-') { if (start || i + 1 == n || s[i + 1] == '.') return false; start = false; continue; }
    if (!alnum) return false;
    start = false;
  }
  return !start;
}

static void domainSet(const char* s, size_t n) {
  s_seq.fetch_add(1);
  if (n) memcpy(s_dom, s, n);
  s_dom[n] = 0;
  s_seq.fetch_add(1);
}

extern "C" void lwip_dhcp_on_extra_option(struct dhcp*, uint8_t state, uint8_t option, uint8_t len, struct pbuf* p, uint16_t offset) {
  if (option != DHCP_OPT_DOMAIN) return;
  if (state != DHCP_STATE_REQUESTING && state != DHCP_STATE_RENEWING && state != DHCP_STATE_REBINDING && state != DHCP_STATE_REBOOTING) return;
  char buf[DOM_MAX + 2];
  if (len == 0 || len > DOM_MAX + 1) return;                           // vuoto o troppo lungo: ignorato
  if (pbuf_copy_partial(p, buf, len, offset) != len) return;
  size_t n = len;
  while (n && buf[n - 1] == 0) n--;                                    // alcuni router chiudono il testo con uno zero
  if (n && buf[n - 1] == '.') n--;                                     // punto finale (nome completo)
  if (n > DOM_MAX || !domainOk(buf, n)) return;
  domainSet(buf, n);
}

size_t drvDhcpDomain(char* out, size_t cap) {
  if (!cap) return 0;
  out[0] = 0;
  char tmp[DOM_MAX + 1];
  for (int tries = 0; tries < 4; tries++) {
    uint32_t a = s_seq.load();
    if (a & 1) continue;
    memcpy(tmp, s_dom, sizeof tmp);
    if (s_seq.load() == a) {
      tmp[DOM_MAX] = 0;
      size_t n = strlen(tmp);
      if (n >= cap) return 0;
      memcpy(out, tmp, n + 1);
      return n;
    }
  }
  return 0;
}

void drvDhcpDomainClear() { domainSet("", 0); }
