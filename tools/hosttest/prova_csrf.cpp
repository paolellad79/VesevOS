// Prova sul computer: controllo CSRF (vos_web_csrf.cpp, il file vero). Uso: tools/hosttest/prova_csrf.sh
#include <stdio.h>
#include "vos_web_csrf.h"
using webx::csrfCrossSite;
static int bad = 0;
static void chk(const char* what, bool got, bool want) { if (got != want) { printf("KO: %s (atteso %s)\n", what, want ? "rifiutata" : "ammessa"); bad++; } }
int main() {
  const char* H = "192.168.110.96";
  // browser moderni: Sec-Fetch-Site
  chk("same-origin", csrfCrossSite("same-origin", "https://192.168.110.96", H), false);
  chk("none (azione dell'utente)", csrfCrossSite("none", nullptr, H), false);
  chk("cross-site", csrfCrossSite("cross-site", "https://evil.example", H), true);
  chk("same-site", csrfCrossSite("same-site", "https://altro.192.168.110.96", H), true);
  chk("valore sconosciuto", csrfCrossSite("boh", nullptr, H), true);
  chk("same-origin con Origin falso: vince Sec-Fetch-Site", csrfCrossSite("same-origin", "https://evil.example", H), false);
  chk("cross-site senza Origin", csrfCrossSite("cross-site", nullptr, H), true);
  // senza Sec-Fetch-Site: Origin contro Host
  chk("Origin uguale a Host", csrfCrossSite(nullptr, "https://192.168.110.96", H), false);
  chk("Origin uguale, maiuscole", csrfCrossSite("", "HTTPS://VESEVOS.LOCAL", "vesevos.local"), false);
  chk("Origin con porta uguale", csrfCrossSite("", "https://vesevos.local:8443", "vesevos.local:8443"), false);
  chk("Origin con porta diversa", csrfCrossSite("", "https://vesevos.local:8443", "vesevos.local"), true);
  chk("Origin di un altro sito", csrfCrossSite("", "https://evil.example", H), true);
  chk("Origin che comincia come Host", csrfCrossSite("", "https://192.168.110.961", H), true);
  chk("Origin piu corto di Host", csrfCrossSite("", "https://192.168.110.9", H), true);
  chk("Origin null", csrfCrossSite("", "null", H), true);
  chk("Origin senza schema", csrfCrossSite("", "192.168.110.96", H), true);
  chk("Origin senza Host", csrfCrossSite("", "https://192.168.110.96", nullptr), true);
  chk("Origin con / finale", csrfCrossSite("", "https://192.168.110.96/", H), false);
  // programmi (aggiorna.py, curl): nessuna delle due intestazioni
  chk("programma senza intestazioni", csrfCrossSite(nullptr, nullptr, H), false);
  chk("programma, stringhe vuote", csrfCrossSite("", "", H), false);
  if (bad) { printf("%d prove fallite\n", bad); return 1; }
  printf("CSRF: tutte le prove OK\n");
  return 0;
}
