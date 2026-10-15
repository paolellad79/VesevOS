#!/bin/bash
# Controllo della sintassi di TUTTI i file del firmware con gli stub (senza la scheda) + ricerca funzioni doppie.
# Serve: g++, e accanto al repo le librerie (solo intestazioni):
#   ../psychic  = https://github.com/hoeken/PsychicHttp (v3.1.2)
#   ../aj       = https://github.com/bblanchon/ArduinoJson (v7)
#   ../mbed     = https://github.com/Mbed-TLS/mbedtls (v3.6, cartella include)
# Uso: tools/stub/compila.sh     (dalla cartella del repo). Package spenti: VOS_EXTRA="-DVOS_WITH_BERRY=0 -DVOS_WITH_BLE=0" tools/stub/compila.sh
R=$(cd "$(dirname "$0")/../.." && pwd); L=$(dirname "$R")
cd "$R/VesevOS"; O=$(mktemp -d); fail=0
for f in $(find . -name "*.cpp" | sed "s|^\./||") VesevOS.ino; do
  out=$(g++ -std=gnu++17 -x c++ -c -w -DARDUINO=10800 -DESP32 $VOS_EXTRA -I"$R/tools/stub" -I"$L/psychic/src" -I"$L/aj/src" -I"$L/mbed/include" "$f" -o "$O/$(basename "$f").o" 2>&1 | grep -E "error|fatal" | head -6)
  if [ -n "$out" ]; then echo "== $f"; echo "$out"; fail=1; fi
done
echo "--- funzioni doppie:"
if [ "$(uname)" = Darwin ]; then  # su Mac nm segna anche le funzioni deboli (inline) come T: si scartano con -m
  nm -m --defined-only "$O"/*.o | grep -v "non-external\|weak\|private external" | grep " external " | sed 's/.* external //;s/ (.*//' | sort | uniq -d | c++filt
else
  nm -C --defined-only "$O"/*.o | grep -E ' [TBDR] ' | awk '{ $1=""; print }' | sort | uniq -d | grep -v "vtable\|typeinfo\|guard\|std::\|__"
fi
[ $fail = 0 ] && echo "ok: tutti i file compilano"
rm -rf "$O"; exit $fail
