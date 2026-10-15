#!/usr/bin/env python3
# VesevOS - ricostruisce VesevOS/src/third_party/berry (interprete Berry, MIT) da una copia di berry-lang/berry.
# Uso:  git clone https://github.com/berry-lang/berry.git /tmp/berry && (cd /tmp/berry && git checkout f333033)
#       python3 tools/mkberry.py /tmp/berry
# Cosa fa: copia src/*.c e src/*.h (senza be_repl.*), la LICENSE, sostituisce i 3 file nostri (tools/berry_local/:
# berry_conf.h = configurazione ridotta, be_port.c = niente file/tastiera, be_modtab.c = solo moduli sicuri)
# e rigenera le tabelle (generate/) con tools/coc di Berry. Versione usata: Berry 1.1.0, commit f333033 (9 ottobre 2026).
import os, shutil, subprocess, sys
HERE = os.path.dirname(os.path.abspath(__file__)); ROOT = os.path.join(HERE, '..')
DST = os.path.join(ROOT, 'VesevOS', 'src', 'third_party', 'berry')
if len(sys.argv) != 2: sys.exit(__doc__)
SRC = sys.argv[1]
if not os.path.isfile(os.path.join(SRC, 'src', 'berry.h')): sys.exit('non trovo %s/src/berry.h' % SRC)
for sub in ('src', 'generate'):
    shutil.rmtree(os.path.join(DST, sub), ignore_errors=True); os.makedirs(os.path.join(DST, sub))
for f in sorted(os.listdir(os.path.join(SRC, 'src'))):
    if f.endswith(('.c', '.h')) and not f.startswith('be_repl'):
        shutil.copy(os.path.join(SRC, 'src', f), os.path.join(DST, 'src', f))
shutil.copy(os.path.join(SRC, 'LICENSE'), os.path.join(DST, 'LICENSE'))
for f in ('berry_conf.h', 'be_port.c', 'be_modtab.c'):
    shutil.copy(os.path.join(HERE, 'berry_local', f), os.path.join(DST, 'src', f))
subprocess.check_call([sys.executable, os.path.join(SRC, 'tools', 'coc', 'coc'), '-o', 'generate', 'src', '-c', 'src/berry_conf.h'], cwd=DST)
# le tabelle generate includono "be_constobj.h" senza percorso: l'originale usa -I src, l'Arduino IDE no
gen = os.path.join(DST, 'generate')
for f in os.listdir(gen):
    t = open(os.path.join(gen, f)).read()
    open(os.path.join(gen, f), 'w', newline='\n').write(t.replace('#include "be_constobj.h"', '#include "../src/be_constobj.h"'))
print('ok: Berry in', os.path.normpath(DST), '-', len(os.listdir(os.path.join(DST, 'generate'))), 'file generati')
