#!/bin/bash
# Prova sul computer: registrazione VERA delle funzioni sd_* (vos_berry_sd.cpp) col codice [cmd] vero di sd-spi.vdb.
# Un sottocomando di sola lettura da non-Admin deve compilare; sd_write da non-Admin = "permesso negato".
# VECCHIO=1 simula il comportamento di prima della 1.8.11a (funzioni di scrittura registrate solo per l'Admin): la prova DEVE fallire.
# Uso: tools/hosttest/prova_sdreg.sh
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"; REPO="$HERE/../.."
OUT="${TMPDIR:-/tmp}/vos_sdreg"; mkdir -p "$OUT/berry"
"$HERE/prova_vdp_cmd.sh" >/dev/null                     # produce ${TMPDIR}/vos_vdpcmd/cmd.be (codice [cmd] vero)
CMD="${TMPDIR:-/tmp}/vos_vdpcmd/cmd.be"
INC="-DportMUX_TYPE=int -I$HERE/fake_idf -I$REPO/VesevOS $(find "$REPO/VesevOS/src" -type d | grep -v third_party | sed 's/^/-I/' | tr '\n' ' ') -I$REPO/VesevOS/src/third_party/berry/src"
SRC="$REPO/VesevOS/src/packages/vos_berry_sd.cpp"
if [ -n "$VECCHIO" ]; then
  sed -e 's|^  needAdmin(vm, "[a-z_]*");$||' -e 's|^  be_regfunc(vm, "sd_mount"|  if (!admin) return;\n  be_regfunc(vm, "sd_mount"|' "$SRC" > "$OUT/vos_berry_sd_vecchio.cpp"
  SRC="$OUT/vos_berry_sd_vecchio.cpp"; INC="-I$REPO/VesevOS/src/packages $INC"
fi
BE="$REPO/VesevOS/src/third_party/berry/src"
for c in "$BE"/*.c; do [ "$OUT/berry/$(basename "$c").o" -nt "$c" ] || gcc -std=gnu99 -O1 -w -I"$BE" -c "$c" -o "$OUT/berry/$(basename "$c").o"; done
g++ -std=gnu++17 -w $INC -c "$SRC" -o "$OUT/sd.o"
g++ -std=gnu++17 -w $INC -c "$HERE/prova_sdreg.cpp" -o "$OUT/prova.o"
g++ "$OUT/prova.o" "$OUT/sd.o" "$OUT"/berry/*.o -lm -o "$OUT/prova"
"$OUT/prova" "$CMD" && echo "funzioni sd_* registrate: tutto OK"
