# VesevOS - controlli sui file prima di scriverli sulla scheda (usato da installa.sh, carica-tutto.sh, carica-firmware.sh).
# Evita lo sbaglio di mettere il RECOVERY nello slot di VesevOS (o il contrario) o di usare il file ".merged.bin".
# Per saltare i controlli (sconsigliato): FORZA=1 ./carica-tutto.sh ...
vos_magic() {   # il file e un programma per ESP32 (formato ESP-IDF)? Byte 0 = 0xE9 e parola ABCD5432 a offset 32 (e la parola dell'intestazione dell'app, valida per ESP-IDF)
  [ "$(od -An -tx1 -N1 "$1" | tr -d ' \n')" = "e9" ] && [ "$(od -An -tx1 -j32 -N4 "$1" | tr -d ' \n')" = "3254cdab" ]
}
vos_chk_fw() {   # $1 = file del FIRMWARE VesevOS (VesevOS-<versione>.bin, per esempio VesevOS-1.7.70g.bin)
  [ -n "$FORZA" ] && return 0
  vos_magic "$1" || { echo "STOP: $1 non e un programma ESP32 valido (e il .merged.bin? serve il file del firmware, per esempio VesevOS-1.7.70g.bin). Per forzare: FORZA=1"; exit 1; }
  grep -aqF "{FW:VesevOS}" "$1" || { echo "STOP: $1 NON e il firmware VesevOS (manca il marchio). Hai scelto il recovery per sbaglio? Serve il file del firmware, per esempio VesevOS-1.7.70g.bin. Per forzare: FORZA=1"; exit 1; }
}
vos_chk_rec() {  # $1 = file del RECOVERY (VesevOS-Recovery-0.5.0.bin)
  [ -n "$FORZA" ] && return 0
  vos_magic "$1" || { echo "STOP: $1 non e un programma ESP32 valido (serve il file del recovery, per esempio VesevOS-Recovery-0.5.0.bin). Per forzare: FORZA=1"; exit 1; }
  grep -aqF "=== VesevOS Recovery" "$1" || { echo "STOP: $1 NON e il recovery (serve il file del recovery, per esempio VesevOS-Recovery-0.5.0.bin). Per forzare: FORZA=1"; exit 1; }
  if grep -aqF "{FW:VesevOS}" "$1"; then echo "STOP: $1 contiene il marchio del firmware: e il file sbagliato. Per forzare: FORZA=1"; exit 1; fi
}
