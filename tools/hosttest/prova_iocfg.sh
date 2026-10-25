#!/bin/bash
# Prova sul computer: come si svuota /io.cfg. Uso: CORE=<...>/cores/esp32 tools/hosttest/prova_iocfg.sh
# Non fa parte di run.sh. Usa un file system finto in memoria (tools/hosttest/iocfg/FS.h al posto di tools/stub/FS.h).
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"; REPO="$HERE/../.."
[ -n "${CORE:-}" ] || { echo "Errore: imposta CORE con la cartella cores/esp32 del nucleo Arduino-ESP32 (esempio: CORE=/percorso/cores/esp32)." >&2; exit 1; }
CORE="$CORE"
OUT="${TMPDIR:-/tmp}/vos_iocfg"; mkdir -p "$OUT"
INC="-I$REPO/VesevOS $(find "$REPO/VesevOS/src" -type d | sed 's/^/-I/' | tr '\n' ' ')"
SAN=""; [ "${ASAN:-1}" = "1" ] && SAN="-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer -g" && OUT="${OUT}_asan" && mkdir -p "$OUT" && export ASAN_OPTIONS=detect_leaks=0
CXX="g++ $SAN -std=gnu++17 -w -DARDUINO=10800 -DESP32 -I$HERE/iocfg -I$REPO/tools/stub $INC -I$HERE -I$OUT"
printf '#pragma once\n#define log_e(...)\n#define log_w(...)\n#define log_i(...)\n#define log_d(...)\n#define log_v(...)\n' > "$OUT/esp32-hal-log.h"
cp "$CORE/Print.cpp" "$CORE/Stream.cpp" "$CORE/WString.cpp" "$CORE/stdlib_noniso.h" "$OUT/"
for f in WString Print Stream; do $CXX -c "$OUT/$f.cpp" -o "$OUT/$f.o"; done
$CXX -c "$HERE/shim.cpp" -o "$OUT/shim.o"
$CXX -c "$HERE/shim_crypto.cpp" -o "$OUT/shimc.o"
for f in vos_util vos_config vos_config_export vos_config_import vos_serial vos_log vos_crypt vos_drv_fs vos_io vos_io_cfg vos_io_name vos_io_text vos_bmp280; do $CXX -c "$(find "$REPO/VesevOS" -name "$f.cpp")" -o "$OUT/$f.o"; done
$CXX -c "$HERE/fake_i2c.cpp" -o "$OUT/fake_i2c.o"
$CXX -c "$HERE/fake_sd.cpp" -o "$OUT/fake_sd.o"
$CXX -c "$HERE/prova_iocfg.cpp" -o "$OUT/prova_iocfg.o"
g++ $SAN "$OUT/prova_iocfg.o" "$OUT/vos_io.o" "$OUT/vos_io_cfg.o" "$OUT/vos_io_name.o" "$OUT/vos_io_text.o" "$OUT/vos_bmp280.o" "$OUT/fake_i2c.o" "$OUT/fake_sd.o" "$OUT"/vos_util.o "$OUT"/vos_config*.o "$OUT"/vos_serial.o "$OUT"/vos_log.o "$OUT"/vos_crypt.o "$OUT"/vos_drv_fs.o "$OUT"/shimc.o "$OUT"/shim.o "$OUT"/WString.o "$OUT"/Print.o "$OUT"/Stream.o -lcrypto -pthread -o "$OUT/prova_iocfg" 2>&1 | grep -E "undefined|error" | head -10 || true
"$OUT/prova_iocfg"
