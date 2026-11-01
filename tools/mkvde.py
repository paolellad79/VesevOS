#!/usr/bin/env python3
# VesevOS - impacchetta un driver ELF (.vde): testo + riga "[elf] N" + i N byte dell'ELF. Formato: docs/progetto/formato-vde.md.
# Uso: python3 tools/mkvde.py <testo.vde.txt> <modulo.elf> <uscita.vde>
import sys
if len(sys.argv) != 4:
    sys.exit("Uso: mkvde.py <testo.vde.txt> <modulo.elf> <uscita.vde>")
testo = open(sys.argv[1], "rb").read().rstrip(b"\n")
elf = open(sys.argv[2], "rb").read()
if not elf.startswith(b"\x7fELF"):
    sys.exit("il secondo file non e un ELF")
for b in testo:
    if b > 126 or (b < 32 and b not in (9, 10)):
        sys.exit("il testo deve essere ASCII")
open(sys.argv[3], "wb").write(testo + b"\n[elf] " + str(len(elf)).encode() + b"\n" + elf)
print("scritto", sys.argv[3], len(testo) + len(elf), "byte (testo", len(testo), "+ ELF", len(elf), ")")
