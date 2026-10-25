#!/bin/bash
# Prova dei controlli degli script di caricamento (recovery/controlli.sh) con file finti. Uso: tools/hosttest/test_scripts.sh
R=$(cd "$(dirname "$0")/../.." && pwd); T=$(mktemp -d); trap 'rm -rf "$T"' EXIT
mkdir "$T/bin"; printf '#!/bin/bash\necho ESPTOOL-CHIAMATO "$@"\n' > "$T/bin/python3"; chmod +x "$T/bin/python3"; export PATH="$T/bin:$PATH"
mk() {  # mk nome testo  -> finto programma ESP32 (E9 ... ABCD5432 a 32) con il testo dentro
  python3() { command python3 "$@"; }
  /usr/bin/env -i PATH=/usr/bin:/bin bash -c "printf '\\xe9\\x03\\x02\\x40' > '$T/$1'; head -c 28 /dev/zero >> '$T/$1'; printf '\\x32\\x54\\xcd\\xab' >> '$T/$1'; head -c 100 /dev/zero >> '$T/$1'; printf '%s' '$2' >> '$T/$1'"
}
mk fw.bin '{FW:VesevOS}'; mk rec.bin '=== VesevOS Recovery %s ==='; mk recfw.bin '=== VesevOS Recovery {FW:VesevOS}'
printf 'niente' > "$T/junk.bin"; printf '\xe9' > "$T/merged.bin"; head -c 200 /dev/zero >> "$T/merged.bin"; printf '{FW:VesevOS}' >> "$T/merged.bin"
n=0; bad=0
t() { # t atteso descrizione comando...
  local exp=$1 d=$2; shift 2; local out; out=$("$@" 2>&1); local rc=$?; n=$((n+1))
  if [ "$exp" = ok ] && echo "$out" | grep -q ESPTOOL-CHIAMATO; then :; elif [ "$exp" = stop ] && echo "$out" | grep -q "^STOP" && ! echo "$out" | grep -q ESPTOOL-CHIAMATO; then :; else echo "KO: $d"; echo "$out" | head -3; bad=$((bad+1)); fi
}
cd "$R/recovery"
mkdir -p "$T/b"; cp "$T/rec.bin" "$T/b/VesevOS_Recovery.bin"; echo x > "$T/b/partition-table.bin"; echo x > "$T/b/bootloader.bin"
t ok   "carica-firmware: firmware vero"            ./carica-firmware.sh /dev/x "$T/fw.bin"
t stop "carica-firmware: RECOVERY al posto del firmware" ./carica-firmware.sh /dev/x "$T/rec.bin"
t stop "carica-firmware: file qualsiasi"            ./carica-firmware.sh /dev/x "$T/junk.bin"
t stop "carica-firmware: merged.bin"                ./carica-firmware.sh /dev/x "$T/merged.bin"
t ok   "carica-firmware: FORZA=1 lascia passare"    env FORZA=1 ./carica-firmware.sh /dev/x "$T/rec.bin"
t ok   "carica-tutto: recovery + firmware giusti"   ./carica-tutto.sh /dev/x "$T/b" "$T/fw.bin"
t stop "carica-tutto: recovery passato come firmware (l'errore del 11/10)" ./carica-tutto.sh /dev/x "$T/b" "$T/b/VesevOS_Recovery.bin"
cp "$T/fw.bin" "$T/b/VesevOS_Recovery.bin"
t stop "carica-tutto: firmware passato come recovery" ./carica-tutto.sh /dev/x "$T/b" "$T/fw.bin"
cp "$T/recfw.bin" "$T/b/VesevOS_Recovery.bin"
t stop "carica-tutto: recovery con il marchio del firmware" ./carica-tutto.sh /dev/x "$T/b" "$T/fw.bin"
cp "$T/rec.bin" "$T/b/VesevOS_Recovery.bin"
t ok   "installa: recovery + firmware giusti"       ./installa.sh /dev/x "$T/b" "$T/fw.bin"
t stop "installa: recovery come firmware"           ./installa.sh /dev/x "$T/b" "$T/rec.bin"
echo "scripts: $n prove, $bad fallite"; [ $bad = 0 ]
