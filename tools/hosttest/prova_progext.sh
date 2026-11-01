#!/bin/bash
# Prova sul computer delle estensioni dei programmi (.vsb/.vab/.vae e i vecchi nomi). Senza Arduino ne scheda. Uso: tools/hosttest/prova_progext.sh
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"; REPO="$HERE/../.."
OUT="${TMPDIR:-/tmp}/vos_progext"; mkdir -p "$OUT"
INC="-I$REPO/VesevOS $(find "$REPO/VesevOS/src" -type d -not -path '*third_party*' | sed 's/^/-I/' | tr '\n' ' ')"
g++ -std=gnu++17 -w -I"$REPO/tools/stub" $INC -I"$HERE" "$HERE/prova_progext.cpp" "$REPO/VesevOS/src/packages/vos_progext.cpp" -o "$OUT/prova_progext" 2>&1 | grep -E 'error|Undefined|^  "|referenced' | head -20 || true
"$OUT/prova_progext"
