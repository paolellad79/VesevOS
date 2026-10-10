#!/bin/bash
# VesevOS - carica SOLO il firmware nello slot app0 (indirizzo 0x110000), senza toccare recovery e impostazioni.
# Uso:  ./carica-firmware.sh PORTA VesevOS.ino.bin
# ATTENZIONE: NON usare il pulsante Carica dell'IDE Arduino per VesevOS: scrive a 0x10000 e cancella il recovery.
set -e
P="$1"; F="$2"
[ -z "$P" ] || [ -z "$F" ] && { sed -n '2,5p' "$0"; exit 1; }
[ -f "$F" ] || { echo "Manca il file: $F"; exit 1; }
sz() { stat -f%z "$1" 2>/dev/null || stat -c%s "$1"; }
[ "$(sz "$F")" -gt 2424832 ] && { echo "STOP: il firmware ($(sz "$F") byte) supera lo slot (2.424.832 byte)."; exit 1; }
python3 -m esptool --chip esp32s3 -p "$P" -b 460800 write_flash 0x110000 "$F"
echo "Fatto. Se la scheda era in VesevOS, riavviala."
