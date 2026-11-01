// Prova sul computer: opzione 15 del DHCP (vos_drv_dhcp.cpp, il file vero) con finti pbuf. Uso: tools/hosttest/prova_dhcp.sh
#include <stdio.h>
#include <string.h>
#include "vos_drv_dhcp.h"
#include "lwip/pbuf.h"
#include "lwip/prot/dhcp.h"
extern "C" void lwip_dhcp_on_extra_option(struct dhcp*, uint8_t state, uint8_t option, uint8_t len, struct pbuf* p, uint16_t offset);
static int bad = 0;
static void opt(uint8_t state, uint8_t option, const char* txt, size_t n) {
  uint8_t buf[300]; buf[0] = 0xAA; memcpy(buf + 1, txt, n);           // 1 byte davanti: l'offset conta
  struct pbuf p = {buf, (uint16_t)(n + 1)};
  lwip_dhcp_on_extra_option(nullptr, state, option, (uint8_t)n, &p, 1);
}
static void give(const char* what, const char* txt, const char* want, uint8_t state = DHCP_STATE_REQUESTING, uint8_t option = 15) {
  drvDhcpDomainClear();
  opt(state, option, txt, strlen(txt));
  char out[64]; size_t n = drvDhcpDomain(out, sizeof out);
  if (strcmp(out, want) || n != strlen(want)) { printf("KO: %s: letto \"%s\" (%zu), atteso \"%s\"\n", what, out, n, want); bad++; }
}
int main() {
  give("dominio semplice", "lan", "lan");
  give("dominio con punti", "fritz.box", "fritz.box");
  give("maiuscole e trattini", "Casa-Mia.example.org", "Casa-Mia.example.org");
  give("punto finale tolto", "lan.", "lan");
  give("rinnovo", "lan", "lan", DHCP_STATE_RENEWING);
  give("rebinding", "lan", "lan", DHCP_STATE_REBINDING);
  give("altro stato ignorato", "lan", "", DHCP_STATE_SELECTING);
  give("altra opzione ignorata", "lan", "", DHCP_STATE_REQUESTING, 119);
  give("spazio non ammesso", "mia rete", "");
  give("underscore non ammesso", "mia_rete", "");
  give("controllo non ammesso", "lan\x01", "");
  give("accento (UTF-8) non ammesso", "citt\xc3\xa0", "");
  give("punto iniziale", ".lan", "");
  give("etichetta vuota", "a..b", "");
  give("trattino a inizio etichetta", "-lan", "");
  give("trattino a fine etichetta", "lan-.it", "");
  give("solo punto", ".", "");
  { drvDhcpDomainClear(); uint8_t b[8] = {0, 'l', 'a', 'n', 0}; struct pbuf p = {b, 5};       // zero finale tolto
    lwip_dhcp_on_extra_option(nullptr, DHCP_STATE_REQUESTING, 15, 4, &p, 1);
    char o[64]; drvDhcpDomain(o, sizeof o); if (strcmp(o, "lan")) { printf("KO: zero finale: \"%s\"\n", o); bad++; } }
  { drvDhcpDomainClear(); char big[100]; memset(big, 'a', sizeof big);                      // troppo lungo: ignorato
    opt(DHCP_STATE_REQUESTING, 15, big, 70); char o[64]; if (drvDhcpDomain(o, sizeof o)) { printf("KO: 70 caratteri accettati\n"); bad++; } }
  { drvDhcpDomainClear(); opt(DHCP_STATE_REQUESTING, 15, "lan", 0); char o[64]; if (drvDhcpDomain(o, sizeof o)) { printf("KO: lunghezza 0 accettata\n"); bad++; } }
  { drvDhcpDomainClear(); opt(DHCP_STATE_REQUESTING, 15, "lan", 3); char o[2]; if (drvDhcpDomain(o, sizeof o)) { printf("KO: buffer piccolo\n"); bad++; } }
  { opt(DHCP_STATE_REQUESTING, 15, "lan", 3); drvDhcpDomainClear(); char o[64]; if (drvDhcpDomain(o, sizeof o)) { printf("KO: Clear non cancella\n"); bad++; } }
  if (bad) { printf("%d prove fallite\n", bad); return 1; }
  printf("DHCP opzione 15: tutte le prove OK\n");
  return 0;
}
