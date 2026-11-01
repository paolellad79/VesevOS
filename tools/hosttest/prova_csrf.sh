#!/bin/bash
# Prova sul computer del controllo CSRF: compila il file VERO vos_web_csrf.cpp. Non serve il nucleo Arduino. Uso: tools/hosttest/prova_csrf.sh
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"; REPO="$HERE/../.."
OUT="${TMPDIR:-/tmp}/vos_csrf"; mkdir -p "$OUT"
g++ -std=gnu++17 -Wall -I"$REPO/VesevOS/src/interfaces" "$HERE/prova_csrf.cpp" "$REPO/VesevOS/src/interfaces/vos_web_csrf.cpp" -o "$OUT/prova"
"$OUT/prova"
