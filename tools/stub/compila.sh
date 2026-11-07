#!/bin/bash
# Controllo della sintassi di TUTTI i file del firmware con gli stub (senza la scheda) + ricerca funzioni doppie.
# Serve: g++, e accanto al repo le librerie (solo intestazioni):
#   ../psychic  = https://github.com/hoeken/PsychicHttp (v3.1.2)
#   ../aj       = https://github.com/bblanchon/ArduinoJson (v7)
#   ../mbed     = https://github.com/Mbed-TLS/mbedtls (v3.6, cartella include)
# Uso: tools/stub/compila.sh     (dalla cartella del repo). Package spenti: VOS_EXTRA="-DVOS_WITH_BERRY=0 -DVOS_WITH_BLE=0" tools/stub/compila.sh
R=$(cd "$(dirname "$0")/../.." && pwd); L=$(dirname "$R")
cd "$R/VesevOS"; O=$(mktemp -d); fail=0
for f in $(find . -name "*.cpp" | sed "s|^\./||"); do   # il punto di ingresso e src/vos_main.cpp
  out=$(g++ -std=gnu++17 -x c++ -c -w -DARDUINO=10800 -DESP32 $VOS_EXTRA -I"$R/tools/stub" -I"$L/psychic/src" -I"$L/aj/src" -I"$L/mbed/include" "$f" -o "$O/$(basename "$f").o" 2>&1 | grep -E "error|fatal" | head -6)
  if [ -n "$out" ]; then echo "== $f"; echo "$out"; fail=1; fi
done
# Secondo giro con i moduli ELF ACCESI: sull'S3 sono di serie (VOS_WITH_ELF=1 arriva da CONFIG_IDF_TARGET_ESP32S3, che qui non c'e) e senza questo giro un
# include mancante dentro "#if VOS_WITH_ELF" non si vedeva (1.8.16: vos_prog_where.cpp). Solo i file che parlano di ELF. vos_drv_elf.cpp include l'intestazione
# di Espressif (esp_elf.h): non c'e fuori da IDF, si salta. Si salta anche se l'utente ha gia scelto VOS_WITH_ELF in VOS_EXTRA.
if ! echo "$VOS_EXTRA" | grep -q "VOS_WITH_ELF"; then
  echo "--- secondo giro con VOS_WITH_ELF=1:"
  mkdir -p "$O/elf"                                      # in una cartella a parte: il controllo delle funzioni doppie guarda solo "$O"/*.o
  for f in $(grep -rlE "VOS_WITH_ELF|vos_elf" --include=*.cpp . | sed "s|^\./||" | grep -v "drivers/vos_drv_elf.cpp"); do
    out=$(g++ -std=gnu++17 -x c++ -c -w -DARDUINO=10800 -DESP32 -DVOS_WITH_ELF=1 $VOS_EXTRA -I"$R/tools/stub" -I"$L/psychic/src" -I"$L/aj/src" -I"$L/mbed/include" "$f" -o "$O/elf/$(basename "$f").o" 2>&1 | grep -E "error|fatal" | head -6)
    if [ -n "$out" ]; then echo "== $f (ELF acceso)"; echo "$out"; fail=1; fi
  done
fi
echo "--- funzioni doppie:"
if [ "$(uname)" = Darwin ]; then  # su Mac nm segna anche le funzioni deboli (inline) come T: si scartano con -m
  nm -m --defined-only "$O"/*.o | grep -v "non-external\|weak\|private external" | grep " external " | sed 's/.* external //;s/ (.*//' | sort | uniq -d | c++filt
else
  nm -C --defined-only "$O"/*.o | grep -E ' [TBDR] ' | awk '{ $1=""; print }' | sort | uniq -d | grep -v "vtable\|typeinfo\|guard\|std::\|__"
fi
[ $fail = 0 ] && echo "ok: tutti i file compilano"
rm -rf "$O"; exit $fail
