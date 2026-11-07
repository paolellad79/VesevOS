#!/bin/bash
# Compila un modulo dei driver ELF (.c) in .elf per ESP32-S3 e lo impacchetta nel .vde con il suo testo.
# Uso: PATH=/usr/bin:$PATH . ~/esp/esp-idf/export.sh ; docs/esempi/driver-elf/build.sh <progetto-mod> <nome>
#   <progetto-mod> = cartella con CMakeLists.txt, main/ e sdkconfig.defaults del progetto dei moduli (esempio: prova-elf/moduli3/mod, che sta fuori dal repo
#   e usa il componente elf_loader di Espressif); <nome> = bmp280 oppure sd-spi (cerca <nome>.c e <nome>.vde.txt in questa cartella).
# Scrive <nome>.elf e <nome>.vde accanto ai sorgenti. Il modulo non usa la libc: solo memcpy, memset, strlen e le funzioni vos_* (vedi vde_util.h).
set -e
AQUI="$(cd "$(dirname "$0")" && pwd)"
PROG="$(cd "$1" && pwd)"; N="$2"
mkdir -p "$PROG/../src"
cp "$AQUI/$N.c" "$AQUI/vde_util.h" "$PROG/../src/"
cd "$PROG"
export MODSRC="$N.c"
B="../b/$N"
idf.py -G 'Unix Makefiles' -B "$B" -DSDKCONFIG="$B/sdkconfig" set-target esp32s3 >"$B.log" 2>&1 || { mkdir -p ../b; idf.py -G 'Unix Makefiles' -B "$B" -DSDKCONFIG="$B/sdkconfig" set-target esp32s3 >"../b/$N.log" 2>&1; }
idf.py -G 'Unix Makefiles' -B "$B" -DSDKCONFIG="$B/sdkconfig" elf >>"../b/$N.log" 2>&1 || { tail -30 "../b/$N.log"; exit 1; }
cp "$B/mod.app.elf" "$AQUI/$N.elf"
python3 "$AQUI/../../../tools/mkvde.py" "$AQUI/$N.vde.txt" "$AQUI/$N.elf" "$AQUI/$N.vde"
