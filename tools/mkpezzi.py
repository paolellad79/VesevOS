#!/usr/bin/env python3
# Copia il caricamento a pezzi della pagina (web/src/js_pezzi.js) nel firmware: la pagina di emergenza usa lo STESSO codice, cosi non ci sono due copie.
# Scrive VesevOS/src/interfaces/vos_pezzi_js.h (solo ASCII, JS senza commenti e spazi inutili: tools/compattajs.py).
# Uso: python3 tools/mkpezzi.py          (scrive il file)   |   python3 tools/mkpezzi.py --controlla   (esce con errore se il file e vecchio)
import os
import sys

from compattajs import compatta_js
from controllojs import ErroreLettura

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, 'web', 'src', 'js_pezzi.js')
DST = os.path.join(ROOT, 'VesevOS', 'src', 'interfaces', 'vos_pezzi_js.h')

TESTA = '''// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_pezzi_js.h
// GENERATO da tools/mkpezzi.py da web/src/js_pezzi.js: non modificare a mano. Il caricamento a pezzi della pagina di emergenza e della pagina vera e
// lo stesso codice (funzione caricaPezzi).
#pragma once

static const char kPezziJs[] = R"JS('''


def genera():
    js = open(SRC, encoding='ascii').read()
    try:
        c = compatta_js(js).strip()
    except ErroreLettura as e:
        print('ERRORE js_pezzi.js: %s' % e)
        sys.exit(1)
    if ')JS"' in c:
        print('ERRORE: la sequenza )JS" compare nel codice')
        sys.exit(1)
    return TESTA + c + ')JS";\n'


nuovo = genera()
if '--controlla' in sys.argv:
    vecchio = open(DST, encoding='ascii').read() if os.path.exists(DST) else ''
    if vecchio != nuovo:
        print('vos_pezzi_js.h non e aggiornato: lanciare python3 tools/mkpezzi.py')
        sys.exit(1)
    print('ok vos_pezzi_js.h aggiornato')
else:
    open(DST, 'w', encoding='ascii').write(nuovo)
    print('ok %s: %d byte di JS' % (DST, len(nuovo)))
