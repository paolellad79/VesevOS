#!/bin/bash
# Prova sul computer del nucleo hardware dei driver ELF (vos_hw.cpp vero, driver finti). Uso: tools/hosttest/prova_hw.sh
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"; REPO="$HERE/../.."
OUT="${TMPDIR:-/tmp}/vos_hw"; mkdir -p "$OUT"
INC="-I$REPO/VesevOS $(find "$REPO/VesevOS/src" -type d -not -path '*third_party*' | sed 's/^/-I/' | tr '\n' ' ') -I$REPO/tools/stub"
g++ -fsanitize=address,undefined -fno-sanitize-recover=all -g -std=gnu++17 -Wall -Wextra -DVOS_WITH_ELF=1 $INC -I"$HERE" \
  "$HERE/prova_hw.cpp" "$REPO/VesevOS/src/packages/vos_hw.cpp" -o "$OUT/prova_hw" 2>&1 | grep -E "error|rror:|Undefined|^  \"" | head -10 || true
ASAN_OPTIONS=detect_leaks=0 "$OUT/prova_hw"
