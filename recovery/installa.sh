#!/bin/bash
# VesevOS - PRIMA installazione con RECOVERY. Da Terminale sul Mac.
# ATTENZIONE: CANCELLA TUTTA la flash (configurazione compresa). Fai prima il backup (read_flash).
# Per aggiornare senza cancellare usa carica-tutto.sh.
# Uso:  ./installa.sh PORTA CARTELLA_RECOVERY [FIRMWARE.bin]
#   PORTA              es. /dev/cu.usbmodem14401
#   CARTELLA_RECOVERY  cartella con VesevOS_Recovery.ino.bin / .bootloader.bin / .partitions.bin (Esporta binario compilato)
#   FIRMWARE.bin       (facoltativo) VesevOS.ino.bin esportato dall'IDE (NON il .merged.bin)
set -e
P="$1"; D="$2"; F="$3"
[ -z "$P" ] || [ -z "$D" ] && { sed -n '2,10p' "$0"; exit 1; }
B="$D/VesevOS_Recovery.ino.bootloader.bin"; T="$D/VesevOS_Recovery.ino.partitions.bin"; R="$D/VesevOS_Recovery.ino.bin"
for f in "$B" "$T" "$R"; do [ -f "$f" ] || { echo "Manca il file: $f"; exit 1; }; done
[ -n "$F" ] && [ ! -f "$F" ] && { echo "Manca il file: $F"; exit 1; }
sz() { stat -f%z "$1" 2>/dev/null || stat -c%s "$1"; }
[ "$(sz "$R")" -gt 1048576 ] && { echo "STOP: il recovery ($(sz "$R") byte) supera 1 MB: non entra nella sua partizione."; exit 1; }
[ -n "$F" ] && [ "$(sz "$F")" -gt 2424832 ] && { echo "STOP: il firmware ($(sz "$F") byte) supera lo slot (2.424.832 byte)."; exit 1; }
E="python3 -m esptool --chip esp32s3 -p $P -b 460800"
echo "== 1/3 cancello la flash"; $E erase_flash
echo "== 2/3 scrivo bootloader, partizioni, recovery"
ARGS="0x0 $B 0x8000 $T 0x10000 $R"
[ -n "$F" ] && ARGS="$ARGS 0x110000 $F"
$E write_flash --flash_mode keep --flash_freq keep --flash_size 4MB $ARGS
echo "== 3/3 fatto. Apri il monitor seriale (115200)."
