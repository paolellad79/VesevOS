#!/bin/bash
# Prova sul computer di Funzionalita e dei gruppi che un'app ELF chiede (vos_func, vos_elf_use, vos_prim). Senza Arduino ne scheda. Uso: tools/hosttest/prova_prog.sh
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"; REPO="$HERE/../.."
OUT="${TMPDIR:-/tmp}/vos_prog"; mkdir -p "$OUT"
INC="-I$REPO/VesevOS $(find "$REPO/VesevOS/src" -type d -not -path '*third_party*' | sed 's/^/-I/' | tr '\n' ' ')"
SAN=""; [ "${ASAN:-1}" = "1" ] && SAN="-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer -g" && export ASAN_OPTIONS=detect_leaks=0
SRC="$REPO/VesevOS/src"
g++ $SAN -std=gnu++17 -Wall -Wextra -DVOS_WITH_ELF=1 -I"$REPO/tools/stub" $INC -I"$HERE" \
  "$HERE/prova_prog.cpp" "$SRC/packages/vos_prim.cpp" "$SRC/packages/vos_func.cpp" "$SRC/packages/vos_elf_use.cpp" -o "$OUT/prova_prog" 2>&1 | grep -E 'error|Undefined|^  "|warning:' | head -20 || true
"$OUT/prova_prog"
