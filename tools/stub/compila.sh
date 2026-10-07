#!/bin/bash
# Controllo della sintassi di TUTTI i file del firmware con gli stub (senza la scheda) + ricerca funzioni doppie.
# Serve: g++, e accanto al repo le librerie (solo intestazioni):
#   ../psychic  = https://github.com/hoeken/PsychicHttp (v3.1.2)
#   ../aj       = https://github.com/bblanchon/ArduinoJson (v7)
#   ../mbed     = https://github.com/Mbed-TLS/mbedtls (v3.6, cartella include)
# Uso: tools/stub/compila.sh     (dalla cartella del repo)
R=$(cd "$(dirname "$0")/../.." && pwd); L=$(dirname "$R")
cd "$R/VesevOS"; O=$(mktemp -d); fail=0
for f in *.cpp VesevOS.ino; do
  out=$(g++ -std=gnu++17 -x c++ -c -w -DARDUINO=10800 -DESP32 -I"$R/tools/stub" -I"$L/psychic/src" -I"$L/aj/src" -I"$L/mbed/include" "$f" -o "$O/$f.o" 2>&1 | grep -E "error|fatal" | head -6)
  if [ -n "$out" ]; then echo "== $f"; echo "$out"; fail=1; fi
done
echo "--- funzioni doppie:"
nm -C --defined-only "$O"/*.o | grep -E ' [TBDR] ' | awk '{ $1=""; print }' | sort | uniq -d | grep -v "vtable\|typeinfo\|guard\|std::\|__"
[ $fail = 0 ] && echo "ok: tutti i file compilano"
rm -rf "$O"; exit $fail
