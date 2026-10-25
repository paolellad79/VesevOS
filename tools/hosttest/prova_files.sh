#!/bin/bash
# Prova sul computer: gestione file e cartelle (core/vos_files.cpp). Uso: CORE=<...>/cores/esp32 tools/hosttest/prova_files.sh
# Usa un file system finto con cartelle (tools/hosttest/dirfs/FS.h al posto di tools/stub/FS.h). run.sh la chiama alla fine.
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"; REPO="$HERE/../.."
[ -n "${CORE:-}" ] || { echo "Errore: imposta CORE con la cartella cores/esp32 del nucleo Arduino-ESP32 (esempio: CORE=/percorso/cores/esp32)." >&2; exit 1; }
CORE="$CORE"
OUT="${TMPDIR:-/tmp}/vos_files"; mkdir -p "$OUT"
INC="-I$REPO/VesevOS $(find "$REPO/VesevOS/src" -type d | sed 's/^/-I/' | tr '\n' ' ')"
SAN=""; [ "${ASAN:-1}" = "1" ] && SAN="-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer -g" && OUT="${OUT}_asan" && mkdir -p "$OUT" && export ASAN_OPTIONS=detect_leaks=0
CXX="g++ $SAN -std=gnu++17 -w -DARDUINO=10800 -DESP32 -I$HERE/dirfs -I$REPO/tools/stub $INC -I$HERE -I$OUT"
printf '#pragma once\n#define log_e(...)\n#define log_w(...)\n#define log_i(...)\n#define log_d(...)\n#define log_v(...)\n' > "$OUT/esp32-hal-log.h"
cp "$CORE/Print.cpp" "$CORE/Stream.cpp" "$CORE/WString.cpp" "$CORE/stdlib_noniso.h" "$OUT/"
for f in WString Print Stream; do $CXX -c "$OUT/$f.cpp" -o "$OUT/$f.o"; done
$CXX -c "$HERE/shim.cpp" -o "$OUT/shim.o"
$CXX -c "$HERE/shim_crypto.cpp" -o "$OUT/shimc.o"
for f in vos_util vos_files vos_drv_fs; do $CXX -c "$(find "$REPO/VesevOS" -name "$f.cpp")" -o "$OUT/$f.o"; done
$CXX -c "$HERE/prova_files.cpp" -o "$OUT/prova_files.o"
g++ $SAN "$OUT/prova_files.o" "$OUT/vos_files.o" "$OUT/vos_util.o" "$OUT/vos_drv_fs.o" "$OUT/shimc.o" "$OUT/shim.o" "$OUT/WString.o" "$OUT/Print.o" "$OUT/Stream.o" -lcrypto -pthread -o "$OUT/prova_files" 2>&1 | grep -E "undefined|error" | head -10 || true
"$OUT/prova_files"
