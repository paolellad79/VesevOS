#!/bin/bash
# Prova sul computer: registrazione VERA delle funzioni VesevOS gpio/i2c/spi/time (vos_funzioni_hw.cpp) con le funzioni VesevOS vero e nucleo hardware finto.
# Uso: tools/hosttest/prova_funzionihw.sh
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"; REPO="$HERE/../.."
OUT="${TMPDIR:-/tmp}/vos_funzionihw"; mkdir -p "$OUT/funzioni"
INC="-DportMUX_TYPE=int -DVOS_WITH_ELF=1 -I$HERE/fake_idf -I$REPO/VesevOS $(find "$REPO/VesevOS/src" -type d | grep -v third_party | sed 's/^/-I/' | tr '\n' ' ') -I$REPO/VesevOS/src/third_party/funzioni/src -I$HERE"
BE="$REPO/VesevOS/src/third_party/funzioni/src"
for c in "$BE"/*.c; do [ "$OUT/funzioni/$(basename "$c").o" -nt "$c" ] || gcc -std=gnu99 -O1 -w -I"$BE" -c "$c" -o "$OUT/funzioni/$(basename "$c").o"; done
g++ -std=gnu++17 -w $INC -c "$REPO/VesevOS/src/packages/vos_funzioni_hw.cpp" -o "$OUT/hw.o"
g++ -std=gnu++17 -w $INC -c "$HERE/prova_funzionihw.cpp" -o "$OUT/prova.o"
g++ "$OUT/prova.o" "$OUT/hw.o" "$OUT"/funzioni/*.o -lm -o "$OUT/prova"
"$OUT/prova"
