#!/bin/bash
# Prova sul computer del formato .vde (vos_vde.cpp e vos_mod_line.cpp veri). Uso: tools/hosttest/prova_vde.sh
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"; REPO="$HERE/../.."
OUT="${TMPDIR:-/tmp}/vos_vde"; mkdir -p "$OUT"
INC="-I$REPO/VesevOS $(find "$REPO/VesevOS/src" -type d -not -path '*third_party*' | sed 's/^/-I/' | tr '\n' ' ') -I$REPO/tools/stub"
SRC="$REPO/VesevOS/src"
g++ -fsanitize=address,undefined -fno-sanitize-recover=all -g -std=gnu++17 -Wall -Wextra -DVOS_WITH_ELF=1 -DREPO="\"$REPO\"" $INC -I"$HERE" \
  "$HERE/prova_vde.cpp" "$SRC/packages/vos_vde.cpp" "$SRC/devices/vos_mod_line.cpp" -o "$OUT/prova_vde" 2>&1 | grep -E "error|rror:|Undefined|^  \"" | head -12 || true
ASAN_OPTIONS=detect_leaks=0 "$OUT/prova_vde"
