#!/usr/bin/env python3
# Assembla web/index.html dai pezzi in web/src/ (ordine in web/src/ORDINE.txt).
# Si modificano i pezzi in web/src/, poi si lancia questo script, poi mklang.py e mkpage.py.
# Uso: python3 tools/mkweb.py          (scrive web/index.html)
#      python3 tools/mkweb.py --check  (esce con errore se index.html non e allineato ai pezzi)
import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import manuale_lib
from versioni_risorse import versione_risorsa
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, 'web', 'src')
names = [l.strip() for l in open(os.path.join(SRC, 'ORDINE.txt'), encoding='utf-8') if l.strip() and not l.startswith('#')]
out = ''.join(open(os.path.join(SRC, n), encoding='utf-8', newline='').read() for n in names)
# Il manuale breve del tab Aiuto non si scrive a mano: arriva da tools/data/manuale_src.txt (stessa fonte di manuale.txt sulla scheda).
MARCA = '<!--MANUALE_BREVE-->'
if out.count(MARCA) != 1: raise SystemExit('mkweb: la marca %s deve comparire una sola volta' % MARCA)
out = out.replace(MARCA, manuale_lib.html_breve(manuale_lib.leggi()).lstrip(' '))
# La versione della pagina e una costante propria (tools/data/versioni_risorse.txt), mai la versione del firmware.
MARCA_VER = '@@PAGINA_VERSIONE@@'
if out.count(MARCA_VER) != 1: raise SystemExit('mkweb: la marca %s deve comparire una sola volta' % MARCA_VER)
out = out.replace(MARCA_VER, versione_risorsa('pagina'))
dst = os.path.join(ROOT, 'web', 'index.html')
if '--check' in sys.argv:
    cur = open(dst, encoding='utf-8', newline='').read()
    if cur != out: print('web/index.html NON allineato ai pezzi di web/src: lancia tools/mkweb.py'); sys.exit(1)
    print('ok: index.html allineato (%d pezzi)' % len(names)); sys.exit(0)
open(dst, 'w', encoding='utf-8', newline='\n').write(out)
print('ok pagina assemblata: %d pezzi, %d righe' % (len(names), out.count('\n')))
