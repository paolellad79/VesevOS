#!/bin/bash
# Prova sul computer del montaggio automatico della microSD: compila vos_sd.cpp VERO con la scheda finta (fake_sd.cpp). Uso: tools/hosttest/prova_sdauto.sh
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"; REPO="$HERE/../.."
OUT="${TMPDIR:-/tmp}/vos_sdauto"; mkdir -p "$OUT"
INC="-I$REPO/tools/stub -I$HERE -I$HERE/fake_idf -I$REPO/VesevOS $(find "$REPO/VesevOS/src" -type d | grep -v third_party | sed 's/^/-I/' | tr '\n' ' ')"
g++ -std=gnu++17 -w $INC "$HERE/prova_sdauto.cpp" "$REPO/VesevOS/src/devices/vos_sd.cpp" "$HERE/fake_sd.cpp" -o "$OUT/prova_sdauto" 2>&1 | grep -E "error|undefined|Undefined|^  \"" | head -20 || true
"$OUT/prova_sdauto"
