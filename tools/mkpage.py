#!/usr/bin/env python3
# Genera la pagina COMPRESSA (gzip) da web/index.html e la scrive in ../build/risorse/origine/pagina.gz.
# Dal passo 5 del piano (15/11/2026) la pagina NON e piu incorporata nel firmware: tools/mkrisorse.py la prende da qui e produce
# /usr/share/vesevos/html.vrs (file della memoria interna). Niente piu vos_page.h ne id nel firmware.
# Prima della compressione toglie commenti e spazi inutili dal solo JavaScript (tools/compattajs.py); index.html resta leggibile.
# Il browser la riceve con "Content-Encoding: gzip". I caratteri speciali della pagina (accenti, simboli) restano UTF-8 veri.
# Uso: python3 tools/mkpage.py     (esce con errore e non scrive nulla se il JS non si puo compattare)
# Poi: python3 tools/mkrisorse.py  (genera i .vrs)
import gzip
import os
import sys

from compattajs import compatta_pagina
from controllojs import ErroreLettura

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
pagina = open(os.path.join(ROOT, 'web', 'index.html'), encoding='utf-8').read()
try:
    compatta = compatta_pagina(pagina)
except ErroreLettura as e:
    print('ERRORE pagina: %s' % e)
    sys.exit(1)
raw = compatta.encode('utf-8')
gz = gzip.compress(raw, 9, mtime=0)
senza = gzip.compress(pagina.encode('utf-8'), 9, mtime=0)
dest = os.path.join(os.path.dirname(ROOT), 'build', 'risorse', 'origine')
os.makedirs(dest, exist_ok=True)
open(os.path.join(dest, 'pagina.gz'), 'wb').write(gz)
print('ok pagina: %d byte, compressa %d byte (senza compattare il JS %d), scritta in %s' % (len(raw), len(gz), len(senza), os.path.join(dest, 'pagina.gz')))
