#!/bin/bash
# Prova sul computer delle regole del tema della pagina (core/vos_tema_rule.h): file valido, parole vietate, escape, pezzi, dimensione.
# Uso: tools/hosttest/prova_tema.sh
set -e
R=$(cd "$(dirname "$0")/../.." && pwd); O=$(mktemp -d)
cat > "$O/prova.cpp" <<'CPP'
#include <stdio.h>
#include <string.h>
#include <string>
#include "core/vos_tema_rule.h"

static int falliti = 0;
static void vero(bool c, const char* nome) { if (!c) { printf("KO %s\n", nome); falliti++; } }

static TemaEsito controlla(const std::string& t, size_t pezzo = 0) {
  TemaScan s;
  temaScanAvvia(s);
  if (pezzo == 0) pezzo = t.size() ? t.size() : 1;
  for (size_t i = 0; i < t.size(); i += pezzo) {
    size_t n = t.size() - i < pezzo ? t.size() - i : pezzo;
    if (temaScanDai(s, t.data() + i, n) != TEMA_OK) break;
  }
  return temaScanFine(s);
}

int main() {
  vero(controlla(":root{--bg:#0b1220;--ac:#3fa7ff}\n.btn{border-radius:999px}\n") == TEMA_OK, "tema normale");
  vero(controlla("html{scroll-behavior:smooth}\n:root[data-theme=light]{--tx:#0b1220}\n") == TEMA_OK, "scroll-behavior e tema chiaro");
  vero(controlla("/* aspetto con linee sottili */ .card>h3{font-weight:600}") == TEMA_OK, "commento e selettore figlio");
  vero(controlla("") == TEMA_VUOTO, "file vuoto");
  vero(controlla(std::string("a\0b", 3)) == TEMA_NUL, "byte nullo");
  vero(controlla("@import 'x.css';") == TEMA_VIETATO, "@import");
  vero(controlla("@IMPORT url(x)") == TEMA_VIETATO, "@IMPORT maiuscolo");
  vero(controlla("body{background:url(http://x/y.png)}") == TEMA_VIETATO, "url(");
  vero(controlla("body{background:URL(data:image/png;base64,AAAA)}") == TEMA_VIETATO, "URL( maiuscolo");
  vero(controlla("a{background:image-set('x.png' 1x)}") == TEMA_VIETATO, "image-set(");
  vero(controlla("@font-face{src:local(x)}") == TEMA_OK, "src: con due punti (local) e permesso");
  vero(controlla("@font-face{font:src(x)}") == TEMA_VIETATO, "src(");
  vero(controlla("a{width:expression(1)}") == TEMA_VIETATO, "expression(");
  vero(controlla("a{background:javascript:alert(1)}") == TEMA_VIETATO, "javascript:");
  vero(controlla("a{-moz-binding:x}") == TEMA_VIETATO, "-moz-binding");
  vero(controlla("a{background:\\75rl(x)}") == TEMA_VIETATO, "escape con barra rovesciata");
  vero(controlla("</style><script>") == TEMA_VIETATO, "tag");
  // la parola vietata spezzata tra due pezzi si trova lo stesso (pezzi da 1, 2, 3, 5 byte)
  const char* frasi[] = {"x{background:url(a)}", "@import x;", "a{b:expression(1)}", "a{b:-moz-binding:x}", "a{b:image-set(x)}"};
  for (size_t k = 0; k < sizeof frasi / sizeof frasi[0]; k++)
    for (size_t pz = 1; pz <= 5; pz++)
      vero(controlla(frasi[k], pz) == TEMA_VIETATO, "parola spezzata tra pezzi");
  // dimensione: 32768 byte vanno bene, uno in piu no
  std::string g(TEMA_MAX, 'a');
  vero(controlla(g) == TEMA_OK, "32768 byte");
  vero(controlla(g + "a") == TEMA_GRANDE, "32769 byte");
  vero(controlla(g, 1000) == TEMA_OK, "32768 byte a pezzi");
  // nomi
  vero(strcmp(temaEsitoNome(TEMA_OK), "ok") == 0 && strcmp(temaEsitoNome(TEMA_VIETATO), "vietato") == 0, "nomi degli esiti");
  printf(falliti ? "tema: %d falliti\n" : "tema: tutto ok\n", falliti);
  return falliti ? 1 : 0;
}
CPP
g++ -std=gnu++17 -Wall -Wextra -fsanitize=address,undefined -I"$R/VesevOS/src" "$O/prova.cpp" -o "$O/prova"
"$O/prova"
rm -rf "$O"
