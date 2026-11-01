#!/bin/bash
# Prova sul computer dell'opzione 15 del DHCP: compila il file VERO vos_drv_dhcp.cpp con finti pbuf. Non serve il nucleo Arduino. Uso: tools/hosttest/prova_dhcp.sh
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"; REPO="$HERE/../.."
OUT="${TMPDIR:-/tmp}/vos_dhcp"; mkdir -p "$OUT"
g++ -std=gnu++17 -Wall -fsanitize=address,undefined -I"$HERE/fake_idf" -I"$REPO/VesevOS/src/drivers" "$HERE/prova_dhcp.cpp" "$REPO/VesevOS/src/drivers/vos_drv_dhcp.cpp" -o "$OUT/prova"
"$OUT/prova"
