#!/bin/bash
# Prove sul computer (senza scheda): compila alcune parti del firmware con finti pezzi e le prova.
# Uso: tools/hosttest/run.sh   (serve g++; il nucleo Arduino-ESP32 si cerca in $CORE, di fabbrica /home/claude/w/core)
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"; REPO="$HERE/../.."
CORE="${CORE:-/home/claude/w/core/cores/esp32}"
OUT="${TMPDIR:-/tmp}/vos_hosttest"; mkdir -p "$OUT"
CXX="g++ -std=gnu++17 -w -DARDUINO=10800 -DESP32 -I$REPO/tools/stub -I$REPO/VesevOS -I$HERE -I$OUT"
# la stringa di Arduino (vera) e due aiuti
printf '#pragma once\n#define log_e(...)\n#define log_w(...)\n#define log_i(...)\n#define log_d(...)\n#define log_v(...)\n' > "$OUT/esp32-hal-log.h"
cp "$CORE/Print.cpp" "$CORE/Stream.cpp" "$OUT/"
cp "$CORE/WString.cpp" "$CORE/stdlib_noniso.h" "$OUT/"   # il C di stdlib_noniso non serve: le funzioni sono in shim.cpp
$CXX -c "$OUT/WString.cpp" -o "$OUT/ws.o"
$CXX -c "$OUT/Print.cpp" -o "$OUT/pr.o"
$CXX -c "$OUT/Stream.cpp" -o "$OUT/st.o"
$CXX -c "$HERE/shim.cpp" -o "$OUT/shim.o"
$CXX -c "$HERE/shim_crypto.cpp" -o "$OUT/shimc.o"
rc=0
# parti del firmware provate (vanno aggiunte qui quando si scrive un nuovo test)
for f in vos_util vos_config vos_serial vos_log vos_crypt; do $CXX -c "$REPO/VesevOS/$f.cpp" -o "$OUT/$f.o"; done
BASE="$OUT/vos_util.o $OUT/vos_config.o $OUT/vos_serial.o $OUT/vos_log.o $OUT/vos_crypt.o $OUT/shimc.o $OUT/shim.o $OUT/ws.o $OUT/pr.o $OUT/st.o"
for t in "$HERE"/test_*.cpp; do
  n=$(basename "$t" .cpp)
  $CXX -c "$t" -o "$OUT/$n.o"
  g++ "$OUT/$n.o" $BASE -lcrypto -o "$OUT/$n" 2>&1 | grep "undefined" | head -5 || true
  "$OUT/$n" || rc=1
done
exit $rc
