#!/bin/bash
# Prova sul computer delle risorse dei driver ELF (devices/vos_drvres.cpp vero, finti). Uso: tools/hosttest/prova_drvres.sh
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"; REPO="$HERE/../.."
OUT="${TMPDIR:-/tmp}/vos_drvres"; mkdir -p "$OUT"
INC="-I$REPO/VesevOS $(find "$REPO/VesevOS/src" -type d -not -path '*third_party*' | sed 's/^/-I/' | tr '\n' ' ') -I$REPO/tools/stub"
g++ -fsanitize=address,undefined -fno-sanitize-recover=all -g -std=gnu++17 -Wall -Wextra -DVOS_WITH_ELF=1 $INC -I"$HERE" \
  "$HERE/prova_drvres.cpp" "$REPO/VesevOS/src/devices/vos_drvres.cpp" -o "$OUT/prova_drvres" 2>&1 | grep -E "error|rror:|Undefined|^  \"" | head -12 || true
ASAN_OPTIONS=detect_leaks=0 "$OUT/prova_drvres"
