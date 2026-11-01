#!/bin/bash
# Prova sul computer del registro delle primitive ELF (gruppi sd e dev). NON usa Arduino: compila i file veri con -DVOS_WITH_ELF=1 e un
# finto hardware dentro prova_prim.cpp. Uso: tools/hosttest/prova_prim.sh
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"; REPO="$HERE/../.."
OUT="${TMPDIR:-/tmp}/vos_prim"; mkdir -p "$OUT"
INC="-I$REPO/VesevOS $(find "$REPO/VesevOS/src" -type d -not -path '*third_party*' | sed 's/^/-I/' | tr '\n' ' ')"
SAN=""; [ "${ASAN:-1}" = "1" ] && SAN="-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer -g" && export ASAN_OPTIONS=detect_leaks=0
SRC="$REPO/VesevOS/src"
g++ $SAN -std=gnu++17 -Wall -Wextra -DVOS_WITH_ELF=1 -I"$REPO/tools/stub" $INC -I"$HERE" \
  "$HERE/prova_prim.cpp" "$SRC/packages/vos_prim.cpp" "$SRC/packages/vos_prim_sd.cpp" "$SRC/packages/vos_prim_dev.cpp" "$SRC/packages/vos_prim_log.cpp" "$SRC/packages/vos_prim_hw.cpp" "$SRC/packages/vos_prim_files.cpp" \
  "$SRC/core/vos_pathres.cpp" "$SRC/core/vos_paths.cpp" "$SRC/devices/vos_sd_path.cpp" -o "$OUT/prova_prim" 2>&1 | grep -E 'error|Undefined|^  "|warning:' | head -20 || true
"$OUT/prova_prim"
