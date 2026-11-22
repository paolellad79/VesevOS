#!/bin/bash
# Prova sul computer del manifest dell'app web (core/vos_manifest_rule.h) e delle icone incorporate (interfaces/vos_icone_data.h).
# Uso: tools/hosttest/prova_pwa.sh
set -e
R=$(cd "$(dirname "$0")/../.." && pwd); O=$(mktemp -d)
python3 "$R/tools/mkicone.py" --controlla > /dev/null
cat > "$O/prova.cpp" <<'CPP'
#include <stdio.h>
#include <string.h>
#include "core/vos_manifest_rule.h"
#include "interfaces/vos_icone_data.h"

static int falliti = 0;
static void vero(bool c, const char* nome) { if (!c) { printf("KO %s\n", nome); falliti++; } }

static bool png(const uint8_t* d, size_t n, unsigned lato) {
  static const uint8_t F[8] = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1a, '\n'};
  if (n < 24 || memcmp(d, F, 8) != 0) return false;
  unsigned w = (d[16] << 24) | (d[17] << 16) | (d[18] << 8) | d[19];
  unsigned a = (d[20] << 24) | (d[21] << 16) | (d[22] << 8) | d[23];
  return w == lato && a == lato;
}

int main(int argc, char** argv) {
  if (argc > 1) {                                  // uso della shell: stampa il manifest per un nome
    char b[640];
    size_t n = manifestJson(b, sizeof b, argv[1]);
    printf("%.*s", (int)n, b);
    return n ? 0 : 1;
  }
  char b[640];
  vero(manifestJson(b, sizeof b, "vesevos1") > 0 && strstr(b, "\"short_name\":\"vesevos1\"") && strstr(b, "\"name\":\"VesevOS vesevos1\""), "nome della scheda");
  vero(manifestJson(b, sizeof b, "") > 0 && strstr(b, "\"short_name\":\"vesevos\""), "nome vuoto: vesevos");
  vero(manifestJson(b, sizeof b, nullptr) > 0 && strstr(b, "\"short_name\":\"vesevos\""), "nome mancante: vesevos");
  vero(manifestJson(b, sizeof b, "a\"b\\c<d>e f") > 0 && strstr(b, "\"short_name\":\"abcdef\""), "caratteri non ammessi tolti");
  vero(manifestJson(b, sizeof b, "x") > 0 && strstr(b, "\"display\":\"standalone\"") && strstr(b, "\"start_url\":\"/\"") && strstr(b, "192x192") && strstr(b, "512x512"), "campi richiesti");
  vero(manifestJson(b, 40, "vesevos1") == 0, "buffer piccolo: 0 e non troncato");
  vero(png(kIcona180, kIcona180Len, 180), "icona 180");
  vero(png(kIcona192, kIcona192Len, 192), "icona 192");
  vero(png(kIcona512, kIcona512Len, 512), "icona 512");
  printf(falliti ? "pwa: %d falliti\n" : "pwa: tutto ok\n", falliti);
  return falliti ? 1 : 0;
}
CPP
g++ -std=gnu++17 -Wall -Wextra -fsanitize=address,undefined -I"$R/VesevOS/src" "$O/prova.cpp" -o "$O/prova"
"$O/prova"
# il JSON deve essere valido e avere i membri che chiede Chrome per installare l'app
"$O/prova" "vesevos2" | python3 -c "
import json,sys
m=json.load(sys.stdin)
assert m['name']=='VesevOS vesevos2' and m['display']=='standalone' and m['start_url']=='/'
t={i['sizes'] for i in m['icons']}
assert {'192x192','512x512'}<=t, t
print('manifest: JSON valido, nome', m['short_name'], ', icone', sorted(t))
"
rm -rf "$O"
