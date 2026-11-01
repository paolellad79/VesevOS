#!/bin/bash
# Prova sul computer di tabella e quarantena dei moduli ELF (file veri). Uso: tools/hosttest/prova_elfrun.sh
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"; REPO="$HERE/../.."
OUT="${TMPDIR:-/tmp}/vos_elfrun"; mkdir -p "$OUT"
INC="-I$REPO/VesevOS $(find "$REPO/VesevOS/src" -type d -not -path '*third_party*' | sed 's/^/-I/' | tr '\n' ' ') -I$REPO/tools/stub"
SRC="$REPO/VesevOS/src/packages"
g++ -fsanitize=address,undefined -fno-sanitize-recover=all -g -std=gnu++17 -Wall -Wextra -DVOS_WITH_ELF=1 $INC "$HERE/prova_elfrun.cpp" "$SRC/vos_elf_tab.cpp" "$SRC/vos_elf_quar.cpp" -o "$OUT/prova_elfrun" 2>&1 | grep -E "error|rror:|Undefined" | head -10 || true
ASAN_OPTIONS=detect_leaks=0 "$OUT/prova_elfrun"
