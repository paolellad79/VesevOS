#!/bin/bash
# Prova sul computer dei percorsi della microSD (core/vos_paths.cpp). Senza Arduino ne scheda. Uso: tools/hosttest/prova_sdpath.sh
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"; REPO="$HERE/../.."
OUT="${TMPDIR:-/tmp}/vos_sdpath"; mkdir -p "$OUT"
INC="-I$REPO/VesevOS $(find "$REPO/VesevOS/src" -type d -not -path '*third_party*' | sed 's/^/-I/' | tr '\n' ' ')"
g++ -std=gnu++17 -w -I"$REPO/tools/stub" $INC -I"$HERE" "$HERE/prova_sdpath.cpp" "$REPO/VesevOS/src/core/vos_paths.cpp" -o "$OUT/prova_sdpath" 2>&1 | grep -E 'error|Undefined|^  "|referenced' | head -20 || true
"$OUT/prova_sdpath"
