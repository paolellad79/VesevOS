#!/bin/bash
# Prova sul computer: comandi dei driver nei file .vdb. Compila i file VERI vos_mod.cpp, vos_mod_line.cpp, vos_mod_cmd.cpp, vos_drvwidget.cpp, vos_drvcmd.cpp con pochi finti (prova_vdp_cmd.cpp).
# Non fa parte di run.sh e non serve il nucleo Arduino. Uso: tools/hosttest/prova_vdp_cmd.sh
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"; REPO="$HERE/../.."
OUT="${TMPDIR:-/tmp}/vos_vdpcmd"; mkdir -p "$OUT"
INC="-I$HERE/fake_idf -I$REPO/VesevOS $(find "$REPO/VesevOS/src" -type d | grep -v third_party | sed 's/^/-I/' | tr '\n' ' ')"
CXX="g++ -std=gnu++17 -w $INC"
for f in vos_mod vos_mod_files vos_mod_line vos_mod_cmd vos_drvwidget vos_drvcmd vos_pacchetto; do $CXX -c "$(find "$REPO/VesevOS" -name "$f.cpp")" -o "$OUT/$f.o"; done
$CXX -c "$HERE/prova_vdp_cmd.cpp" -o "$OUT/prova.o"
g++ "$OUT"/prova.o "$OUT"/vos_mod.o "$OUT"/vos_mod_files.o "$OUT"/vos_mod_line.o "$OUT"/vos_mod_cmd.o "$OUT"/vos_drvwidget.o "$OUT"/vos_drvcmd.o "$OUT"/vos_pacchetto.o -o "$OUT/prova"
VOS_REPO="$(cd "$REPO" && pwd)" "$OUT/prova"
# Il codice [cmd] estratto dal vero modCmdCode gira nell'interprete delle funzioni vero, con una microSD finta in memoria (prova_vdp_cmd_*.be)
BE="$REPO/VesevOS/src/third_party/funzioni/src"; mkdir -p "$OUT/funzioni"
for c in "$BE"/*.c; do [ "$OUT/funzioni/$(basename "$c").o" -nt "$c" ] || gcc -std=gnu99 -O1 -w -I"$BE" -c "$c" -o "$OUT/funzioni/$(basename "$c").o"; done
gcc -std=gnu99 -w -I"$BE" -c "$HERE/funzioni_run.c" -o "$OUT/funzioni_run.o"
gcc "$OUT/funzioni_run.o" "$OUT"/funzioni/*.o -lm -o "$OUT/funzioni_run"
VDP_CMD_OUT="$OUT/cmd.be" VOS_REPO="$(cd "$REPO" && pwd)" "$OUT/prova" >/dev/null
cat "$HERE/prova_vdp_cmd_pre.be" "$OUT/cmd.be" "$HERE/prova_vdp_cmd_chk.be" > "$OUT/tutto.be"
"$OUT/funzioni_run" "$OUT/tutto.be" > "$OUT/uscita.txt" || { cat "$OUT/uscita.txt"; echo "KO: errore nelle funzioni VesevOS"; exit 1; }
if diff "$HERE/prova_vdp_cmd_atteso.txt" "$OUT/uscita.txt"; then echo "comandi funzioni VesevOS del driver: tutto OK"; else echo "KO: uscita diversa da quella attesa"; exit 1; fi
