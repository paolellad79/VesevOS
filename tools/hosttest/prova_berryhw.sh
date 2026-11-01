#!/bin/bash
# Prova sul computer: registrazione VERA delle funzioni Berry gpio/i2c/spi/time (vos_berry_hw.cpp) con Berry vero e nucleo hardware finto.
# Uso: tools/hosttest/prova_berryhw.sh
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"; REPO="$HERE/../.."
OUT="${TMPDIR:-/tmp}/vos_berryhw"; mkdir -p "$OUT/berry"
INC="-DportMUX_TYPE=int -DVOS_WITH_ELF=1 -I$HERE/fake_idf -I$REPO/VesevOS $(find "$REPO/VesevOS/src" -type d | grep -v third_party | sed 's/^/-I/' | tr '\n' ' ') -I$REPO/VesevOS/src/third_party/berry/src -I$HERE"
BE="$REPO/VesevOS/src/third_party/berry/src"
for c in "$BE"/*.c; do [ "$OUT/berry/$(basename "$c").o" -nt "$c" ] || gcc -std=gnu99 -O1 -w -I"$BE" -c "$c" -o "$OUT/berry/$(basename "$c").o"; done
g++ -std=gnu++17 -w $INC -c "$REPO/VesevOS/src/packages/vos_berry_hw.cpp" -o "$OUT/hw.o"
g++ -std=gnu++17 -w $INC -c "$HERE/prova_berryhw.cpp" -o "$OUT/prova.o"
g++ "$OUT/prova.o" "$OUT/hw.o" "$OUT"/berry/*.o -lm -o "$OUT/prova"
"$OUT/prova"
