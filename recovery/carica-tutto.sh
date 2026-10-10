#!/bin/bash
# VesevOS - aggiorna tabella partizioni (0x8000), recovery (0x10000) e firmware (0x110000) SENZA cancellare la flash:
# impostazioni e configurazione (LittleFS, NVS) restano.
# Uso:  ./carica-tutto.sh PORTA CARTELLA_RECOVERY FIRMWARE.bin
set -e
P="$1"; D="$2"; F="$3"
[ -z "$P" ] || [ -z "$D" ] || [ -z "$F" ] && { sed -n '2,5p' "$0"; exit 1; }
R="$D/VesevOS_Recovery.ino.bin"; T="$D/VesevOS_Recovery.ino.partitions.bin"
for f in "$R" "$T" "$F"; do [ -f "$f" ] || { echo "Manca il file: $f"; exit 1; }; done
sz() { stat -f%z "$1" 2>/dev/null || stat -c%s "$1"; }
[ "$(sz "$R")" -gt 1048576 ] && { echo "STOP: il recovery ($(sz "$R") byte) supera 1 MB: non entra nella sua partizione."; exit 1; }
[ "$(sz "$F")" -gt 2424832 ] && { echo "STOP: il firmware ($(sz "$F") byte) supera lo slot (2.424.832 byte)."; exit 1; }
python3 -m esptool --chip esp32s3 -p "$P" -b 460800 write_flash 0x8000 "$T" 0x10000 "$R" 0x110000 "$F"
echo "Fatto. Riavvia la scheda."
