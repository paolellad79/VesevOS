#!/bin/bash
# Prova sul computer della configurazione dei programmi (sezioni vdb, func, progboot). Senza Arduino ne scheda. Uso: tools/hosttest/prova_cfgprog.sh
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"; REPO="$HERE/../.."
OUT="${TMPDIR:-/tmp}/vos_cfgprog"; mkdir -p "$OUT"
INC="-I$REPO/VesevOS $(find "$REPO/VesevOS/src" -type d -not -path '*third_party*' | sed 's/^/-I/' | tr '\n' ' ')"
SAN=""; [ "${ASAN:-1}" = "1" ] && SAN="-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer -g" && export ASAN_OPTIONS=detect_leaks=0
SRC="$REPO/VesevOS/src/core"
g++ $SAN -std=gnu++17 -w -DVOS_WITH_ELF=1 -I"$REPO/tools/stub" $INC -I"$HERE" \
  "$HERE/prova_cfgprog.cpp" "$SRC/vos_config.cpp" "$SRC/vos_config_import.cpp" "$SRC/vos_config_export.cpp" -o "$OUT/prova_cfgprog" 2>&1 | grep -E 'error|Undefined|^  "|referenced' | head -30 || true
"$OUT/prova_cfgprog"
