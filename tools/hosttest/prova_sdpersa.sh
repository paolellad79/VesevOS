#!/bin/bash
# Prova sul computer del controllo "microSD sparita" (packages/vos_prog_where.cpp). Senza Arduino ne scheda. Uso: tools/hosttest/prova_sdpersa.sh
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"; REPO="$HERE/../.."
OUT="${TMPDIR:-/tmp}/vos_sdpersa"; mkdir -p "$OUT"
INC="-I$REPO/VesevOS $(find "$REPO/VesevOS/src" -type d -not -path '*third_party*' | sed 's/^/-I/' | tr '\n' ' ')"
SAN=""; [ "${ASAN:-1}" = "1" ] && SAN="-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer -g" && export ASAN_OPTIONS=detect_leaks=0
g++ $SAN -O0 -std=gnu++17 -w -DVOS_WITH_ELF=0 -DVOS_WITH_SD=1 -I"$REPO/tools/stub" $INC -I"$HERE" "$HERE/prova_sdpersa.cpp" "$HERE/prova_sdpersa_fake.cpp" \
  "$REPO/VesevOS/src/packages/vos_prog_where.cpp" -o "$OUT/prova_sdpersa" 2>&1 | grep -E 'error|Undefined|^  "|referenced' | head -30 || true
"$OUT/prova_sdpersa"
