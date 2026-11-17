#!/usr/bin/env python3
# VesevOS - ricostruisce VesevOS/src/third_party/funzioni (interprete delle funzioni VesevOS, MIT) da una copia dell'originale.
# Uso:  python3 tools/mkfunzioni.py <cartella-copia-originale> <nome-del-progetto-originale>
#       (la copia e il commit usati sono scritti in VesevOS/src/third_party/funzioni/README-VesevOS.txt)
# Cosa fa: copia src/*.c e src/*.h (senza be_repl.*), la LICENSE, cambia il nome del progetto originale nei file
# (nomi dei file, macro, simboli, commenti: NON le righe dell'avviso di copyright), sostituisce i 3 file nostri
# (tools/funzioni_locale/: funzioni_conf.h = configurazione ridotta, be_port.c = niente file/tastiera,
# be_modtab.c = solo moduli sicuri) e rigenera le tabelle (generate/) con lo strumento coc dell'originale.
# Il nome originale si passa a riga di comando: cosi non resta scritto nei nostri sorgenti.
import os, re, shutil, subprocess, sys
HERE = os.path.dirname(os.path.abspath(__file__)); ROOT = os.path.join(HERE, '..')
DST = os.path.join(ROOT, 'VesevOS', 'src', 'third_party', 'funzioni')
if len(sys.argv) != 3: sys.exit(__doc__)
SRC = sys.argv[1]
ORIG = sys.argv[2].lower()
if not os.path.isfile(os.path.join(SRC, 'src', ORIG + '.h')): sys.exit('non trovo %s/src/%s.h' % (SRC, ORIG))
# righe dell'avviso di copyright e della licenza: non si toccano mai
AVVISO = ('Skiars', 'skiars@qq.com', 'This file is part of')

def rinomina(testo):
    righe = []
    for riga in testo.split('\n'):
        if any(p in riga for p in AVVISO):
            righe.append(riga)
            continue
        riga = riga.replace(ORIG + '_port', 'be_port')
        riga = riga.replace(ORIG.upper(), 'FUNZIONI')
        riga = riga.replace(ORIG.capitalize(), 'script')
        riga = riga.replace(ORIG, 'funzioni')
        righe.append(riga)
    return '\n'.join(righe)

for sub in ('src', 'generate'):
    shutil.rmtree(os.path.join(DST, sub), ignore_errors=True); os.makedirs(os.path.join(DST, sub))
for f in sorted(os.listdir(os.path.join(SRC, 'src'))):
    if f.endswith(('.c', '.h')) and not f.startswith('be_repl'):
        nome = f.replace(ORIG, 'funzioni')
        testo = open(os.path.join(SRC, 'src', f), encoding='utf-8', errors='surrogateescape').read()
        open(os.path.join(DST, 'src', nome), 'w', newline='\n', encoding='utf-8', errors='surrogateescape').write(rinomina(testo))
shutil.copy(os.path.join(SRC, 'LICENSE'), os.path.join(DST, 'LICENSE'))
for f in ('funzioni_conf.h', 'be_port.c', 'be_modtab.c'):
    shutil.copy(os.path.join(HERE, 'funzioni_locale', f), os.path.join(DST, 'src', f))
subprocess.check_call([sys.executable, os.path.join(SRC, 'tools', 'coc', 'coc'), '-o', 'generate', 'src', '-c', 'src/funzioni_conf.h'], cwd=DST)
# le tabelle generate includono "be_constobj.h" senza percorso: l'originale usa -I src, l'Arduino IDE no
gen = os.path.join(DST, 'generate')
for f in os.listdir(gen):
    t = open(os.path.join(gen, f)).read()
    open(os.path.join(gen, f), 'w', newline='\n').write(t.replace('#include "be_constobj.h"', '#include "../src/be_constobj.h"'))
print('ok: funzioni in', os.path.normpath(DST), '-', len(os.listdir(os.path.join(DST, 'generate'))), 'file generati')
