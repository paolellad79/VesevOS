#!/bin/bash
# Prova di parita dei formati di data e ora: firmware (vos_datefmt.cpp, compilato vero) e pagina (js_dataora.js, con node)
# leggono gli stessi casi da casi_formato_data.txt. Non serve il nucleo Arduino. Uso: tools/hosttest/prova_formato_data.sh
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"; REPO="$HERE/../.."
OUT="${TMPDIR:-/tmp}/vos_formato_data"; mkdir -p "$OUT"
g++ -std=gnu++17 -Wall -fsanitize=address,undefined -I"$HERE" -I"$REPO/VesevOS/src/core" "$HERE/prova_formato_data.cpp" "$REPO/VesevOS/src/core/vos_datefmt.cpp" -o "$OUT/prova"
"$OUT/prova" "$HERE/casi_formato_data.txt"
node "$HERE/prova_formato_data.js"
