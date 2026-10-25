#!/bin/bash
# Prove sul computer (senza scheda): compila alcune parti del firmware con finti pezzi e le prova.
# Uso: tools/hosttest/run.sh   (serve g++; il nucleo Arduino-ESP32 va indicato in $CORE, obbligatorio)
#      ASan + UBSan sono attivi di fabbrica (AddressSanitizer + UBSan: un errore di memoria o un comportamento indefinito ferma la prova = KO).
#      ASAN=0 tools/hosttest/run.sh li spegne (circa 10% piu veloce).
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"; REPO="$HERE/../.."
[ -n "${CORE:-}" ] || { echo "Errore: imposta CORE con la cartella cores/esp32 del nucleo Arduino-ESP32 (esempio: CORE=/percorso/cores/esp32)." >&2; exit 1; }
CORE="$CORE"
OUT="${TMPDIR:-/tmp}/vos_hosttest"; mkdir -p "$OUT"
SAN=""
if [ "${ASAN:-1}" = "1" ]; then SAN="-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer -g"; export ASAN_OPTIONS="detect_leaks=0:abort_on_error=0"; export UBSAN_OPTIONS="print_stacktrace=1"; echo "(ASan+UBSan attivi)"; OUT="${OUT}_asan"; mkdir -p "$OUT"; fi
INC="-I$REPO/VesevOS $(find "$REPO/VesevOS/src" -type d | sed 's/^/-I/' | tr '\n' ' ')"
CXX="g++ $SAN -std=gnu++17 -w -DARDUINO=10800 -DESP32 -I$REPO/tools/stub $INC -I$HERE -I$OUT"
# la stringa di Arduino (vera) e due aiuti
printf '#pragma once\n#define log_e(...)\n#define log_w(...)\n#define log_i(...)\n#define log_d(...)\n#define log_v(...)\n' > "$OUT/esp32-hal-log.h"
cp "$CORE/Print.cpp" "$CORE/Stream.cpp" "$OUT/"
cp "$CORE/WString.cpp" "$CORE/stdlib_noniso.h" "$OUT/"   # il C di stdlib_noniso non serve: le funzioni sono in shim.cpp
$CXX -c "$OUT/WString.cpp" -o "$OUT/ws.o"
$CXX -c "$OUT/Print.cpp" -o "$OUT/pr.o"
$CXX -c "$OUT/Stream.cpp" -o "$OUT/st.o"
$CXX -c "$HERE/shim.cpp" -o "$OUT/shim.o"
$CXX -c "$HERE/shim_crypto.cpp" -o "$OUT/shimc.o"
export VOS_REPO="$(cd "$REPO" && pwd)"
rc=0
# il marchio del firmware non deve comparire in chiaro nel recovery (altrimenti si potrebbe caricare il recovery al posto del firmware)
if grep -rqF --include="*.ino" --include="*.h" --include="*.cpp" "{FW:VesevOS}" "$REPO/recovery"; then echo "KO: marchio in chiaro nel recovery"; rc=1; fi
# e al contrario: il marchio del recovery non deve comparire in chiaro nel firmware (altrimenti il firmware passerebbe per un recovery)
if grep -rqF --include="*.ino" --include="*.h" --include="*.cpp" "{REC:VesevOS:" "$REPO/VesevOS"; then echo "KO: marchio del recovery in chiaro nel firmware"; rc=1; fi
# parti del firmware provate (vanno aggiunte qui quando si scrive un nuovo test)
for f in vos_util vos_config vos_config_export vos_config_import vos_serial vos_log vos_crypt vos_sechash vos_ble_cfg vos_service vos_eventbus vos_drv_fs vos_app_rt vos_io vos_io_cfg vos_io_name vos_io_text vos_bmp280 vos_pins vos_sha256_sw vos_license vos_cpugov; do $CXX -c "$(find "$REPO/VesevOS" -name "$f.cpp")" -o "$OUT/$f.o"; done
$CXX -c "$HERE/fake_i2c.cpp" -o "$OUT/fake_i2c.o"
$CXX -c "$HERE/fake_sd.cpp" -o "$OUT/fake_sd.o"
# logica comune ai backend della microSD (rinominata drvSdProve: il finto ha la sua, il test di contratto prova entrambe)
$CXX -DdrvSdProve=drvSdProveCommon -c "$REPO/VesevOS/src/drivers/vos_drv_sd_common.cpp" -o "$OUT/vos_drv_sd_common.o"
# test_io e test_board: ioAdd/ioRemove/ioRename ora rispondono errore se io.cfg non si scrive, quindi serve un file system finto che scrive davvero (iocfg/FS.h).
# Tutti i pezzi che toccano il file system si compilano di nuovo con quel FS.h (cartella fsx), per non mescolare due definizioni della stessa classe.
FX="$OUT/fsx"; mkdir -p "$FX"
CXXF="g++ $SAN -std=gnu++17 -w -DARDUINO=10800 -DESP32 -I$HERE/iocfg -I$REPO/tools/stub $INC -I$HERE -I$OUT"
$CXXF -c "$HERE/shim.cpp" -o "$FX/shim.o"; $CXXF -c "$HERE/shim_crypto.cpp" -o "$FX/shimc.o"
for f in vos_util vos_config vos_config_export vos_config_import vos_serial vos_log vos_crypt vos_sechash vos_drv_fs vos_io vos_io_cfg vos_io_name vos_io_text vos_bmp280 vos_pins vos_sd vos_sd_act; do $CXXF -c "$(find "$REPO/VesevOS" -name "$f.cpp")" -o "$FX/$f.o"; done
BASEF="$FX/vos_util.o $FX/vos_config.o $FX/vos_config_export.o $FX/vos_config_import.o $FX/vos_serial.o $FX/vos_log.o $FX/vos_crypt.o $FX/vos_sechash.o $FX/vos_drv_fs.o $FX/shimc.o $FX/shim.o $OUT/ws.o $OUT/pr.o $OUT/st.o"
BASE="$OUT/vos_util.o $OUT/vos_config.o $OUT/vos_config_export.o $OUT/vos_config_import.o $OUT/vos_serial.o $OUT/vos_log.o $OUT/vos_crypt.o $OUT/vos_sechash.o $OUT/vos_drv_fs.o $OUT/shimc.o $OUT/shim.o $OUT/ws.o $OUT/pr.o $OUT/st.o"
# interprete Berry (C) per test_berry (compilato SENZA -I, come fa l Arduino IDE: gli include devono trovarsi da soli)
BE="$REPO/VesevOS/src/third_party/berry/src"; mkdir -p "$OUT/berry"
for c in "$BE"/*.c; do gcc $SAN -std=gnu99 -O1 -w -c "$c" -o "$OUT/berry/$(basename "$c").o"; done
for t in "$HERE"/test_*.cpp; do
  n=$(basename "$t" .cpp)
  if [ "$n" = "test_io" ] || [ "$n" = "test_board" ] || [ "$n" = "test_sd" ]; then $CXXF -c "$t" -o "$OUT/$n.o"; else $CXX -c "$t" -o "$OUT/$n.o"; fi
  X=""; [ "$n" = "test_blecfg" ] && X="$OUT/vos_ble_cfg.o"; [ "$n" = "test_service" ] && X="$OUT/vos_service.o $OUT/vos_eventbus.o"; [ "$n" = "test_wps" ] && X="$OUT/vos_service.o $OUT/vos_eventbus.o"; [ "$n" = "test_btn" ] && X="$OUT/vos_eventbus.o"; [ "$n" = "test_eventbus" ] && X="$OUT/vos_eventbus.o"; [ "$n" = "test_berry" ] && X="$OUT/berry/*.o $OUT/vos_app_rt.o"; [ "$n" = "test_app" ] && X="$OUT/vos_app_rt.o"; [ "$n" = "test_io" ] && X="$OUT/vos_io.o $OUT/vos_bmp280.o $OUT/fake_i2c.o"; [ "$n" = "test_board" ] && X="$OUT/vos_io.o $OUT/vos_pins.o $OUT/vos_bmp280.o $OUT/fake_i2c.o"; [ "$n" = "test_bmp280" ] && X="$OUT/vos_bmp280.o $OUT/fake_i2c.o"
  B="$BASE"
  if [ "$n" = "test_io" ] || [ "$n" = "test_board" ]; then
    X="$FX/vos_io.o $FX/vos_io_cfg.o $FX/vos_io_name.o $FX/vos_io_text.o $FX/vos_bmp280.o $OUT/fake_i2c.o $OUT/fake_sd.o"; [ "$n" = "test_board" ] && X="$FX/vos_io.o $FX/vos_io_cfg.o $FX/vos_io_name.o $FX/vos_io_text.o $FX/vos_pins.o $FX/vos_bmp280.o $OUT/fake_i2c.o $OUT/fake_sd.o"; B="$BASEF"
  fi
  if [ "$n" = "test_cpugov" ]; then X="$OUT/vos_cpugov.o"; fi
  if [ "$n" = "test_sha256sw" ]; then X="$OUT/vos_sha256_sw.o"; fi
  if [ "$n" = "test_strbuf" ]; then X="$OUT/vos_license.o"; fi
  if [ "$n" = "test_sd_contract" ]; then X="$OUT/fake_sd.o $OUT/vos_drv_sd_common.o"; fi
  if [ "$n" = "test_sd" ]; then X="$FX/vos_sd.o $FX/vos_sd_act.o $FX/vos_io.o $FX/vos_io_cfg.o $FX/vos_io_name.o $FX/vos_io_text.o $FX/vos_pins.o $FX/vos_bmp280.o $OUT/fake_i2c.o $OUT/fake_sd.o"; B="$BASEF"; fi
  g++ $SAN "$OUT/$n.o" $X $B -lcrypto -pthread -o "$OUT/$n" 2>&1 | grep "undefined" | head -5 || true
  "$OUT/$n" || rc=1
done
"$(dirname "$0")/test_scripts.sh" || rc=1
# gestione file e cartelle: ha il suo file system finto con cartelle (dirfs), per questo si compila a parte
CORE="$CORE" ASAN="${ASAN:-1}" "$HERE/prova_files.sh" || rc=1
exit $rc
