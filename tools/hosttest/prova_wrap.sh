#!/bin/bash
# Prova sul computer: mappe e liste restituite da funzioni native devono essere leggibili dallo script. Uso: tools/hosttest/prova_wrap.sh
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"; REPO="$HERE/../.."
OUT="${TMPDIR:-/tmp}/vos_wrap"; mkdir -p "$OUT/berry"
BE="$REPO/VesevOS/src/third_party/berry/src"
for c in "$BE"/*.c; do [ "$OUT/berry/$(basename "$c").o" -nt "$c" ] || gcc -std=gnu99 -O1 -w -I"$BE" -c "$c" -o "$OUT/berry/$(basename "$c").o"; done
gcc -std=gnu99 -w -I"$BE" -c "$HERE/prova_wrap.c" -o "$OUT/prova_wrap.o"
gcc "$OUT/prova_wrap.o" "$OUT"/berry/*.o -lm -o "$OUT/prova_wrap"
"$OUT/prova_wrap" && echo "mappe e liste native: tutto OK"
