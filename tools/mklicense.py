#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# Genera: VesevOS/vos_license_data.h (testi legali nel firmware, solo ASCII) e NOTICE.txt (copia leggibile).
# Il nome del titolare sta in tools/owner.json. Con --headers aggiorna anche la riga Copyright
# in testa ai sorgenti (solo se il nome e stato impostato).
# Uso: python3 tools/mklicense.py [--headers]
import os, sys, json, glob, re, gzip
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
own = json.load(open(os.path.join(ROOT, 'tools', 'owner.json'), encoding='utf-8'))
placeholder = own['name'].startswith('[')
if placeholder:
    print('ATTENZIONE: il nome del titolare non e ancora impostato in tools/owner.json')

def rd(p):
    t = open(os.path.join(ROOT, p), encoding='ascii').read().replace('\r\n', '\n')
    return t
notice = rd('licenses/NOTICE.template.txt')
for k, v in (('{NAME}', own['name']), ('{YEAR}', own['year']), ('{EMAIL}', own['email']), ('{REPO}', own['repo'])):
    notice = notice.replace(k, v)
for ch in notice:
    if ord(ch) > 127: sys.exit('NOTICE: carattere non ASCII (usa il nome senza accenti nel codice): %r' % ch)
open(os.path.join(ROOT, 'NOTICE.txt'), 'w', newline='\n').write(notice)

DOCS = [  # id, titolo, testo
    ('notice', 'Note legali e licenza di VesevOS', notice),
    ('gpl3',   'GNU General Public License v3.0', rd('licenses/GPL-3.0.txt')),
    ('lgpl21', 'GNU Lesser General Public License v2.1', rd('licenses/LGPL-2.1.txt')),
    ('apache2', 'Apache License 2.0', rd('licenses/Apache-2.0.txt')),
    ('mit',    'MIT License (PsychicHttp, ArduinoJson, FreeRTOS, posix_tz_db)', rd('licenses/MIT.txt')),
    ('bsd3',   'BSD 3-Clause License (lwIP, LittleFS)', rd('licenses/BSD-3-Clause.txt')),
]
def cstr(t, step=7000):
    assert ')VOSLIC"' not in t
    out, i = [], 0
    while i < len(t):
        j = min(i + step, len(t))
        if j < len(t):
            k = t.rfind('\n', i, j)
            j = k + 1 if k > i else j
        out.append('R"VOSLIC(%s)VOSLIC"' % t[i:j]); i = j
    return '\n'.join(out)
h = ['// VesevOS - vos_license_data.h (GENERATO da tools/mklicense.py, solo ASCII)', '#pragma once', '#include <Arduino.h>', '']
# note legali: testo semplice (si legge anche dalla seriale). Le licenze lunghe: gzip (la pagina le riceve compresse, circa 50 KB di flash in meno).
ZS = {}
for i, (id_, title, text) in enumerate(DOCS):
    if id_ == 'notice':
        h.append('static const char LIC_TEXT_%d[] PROGMEM =\n%s;' % (i, cstr(text)))
        ZS[i] = 0
    else:
        z = gzip.compress(text.encode('ascii'), 9, mtime=0)
        ZS[i] = len(z)
        h.append('static const uint8_t LIC_TEXT_%d[] PROGMEM = {' % i)
        for k in range(0, len(z), 24):
            h.append('  ' + ','.join('0x%02x' % c for c in z[k:k+24]) + ',')
        h.append('};')
h.append('')
h.append('struct LicDoc { const char* id; const char* title; const char* text; size_t size; size_t zsize; };')
h.append('static const LicDoc LIC_DOCS[] = {')
for i, (id_, title, text) in enumerate(DOCS):
    h.append('  { "%s", "%s", (const char*)LIC_TEXT_%d, %d, %d },' % (id_, title, i, len(text.encode('ascii')), ZS[i]))
h.append('};')
h.append('#define LIC_COUNT %d' % len(DOCS))
open(os.path.join(ROOT, 'VesevOS', 'vos_license_data.h'), 'w', newline='\n', encoding='ascii').write('\n'.join(h) + '\n')
print('vos_license_data.h: %d documenti, %d byte di testo' % (len(DOCS), sum(len(d[2]) for d in DOCS)))

if '--headers' in sys.argv:
    if placeholder: sys.exit('Imposta prima il nome in tools/owner.json')
    line = '// Copyright (C) %s %s' % (own['year'], own['name'])
    n = 0
    for f in sorted(glob.glob(os.path.join(ROOT, 'VesevOS', '*.cpp')) + glob.glob(os.path.join(ROOT, 'VesevOS', '*.h')) + glob.glob(os.path.join(ROOT, 'VesevOS', '*.ino'))):
        if os.path.basename(f) in ('vos_page.h', 'vos_license_data.h'): continue
        L = open(f, encoding='ascii').read().split('\n')
        if len(L) > 1 and L[1].startswith('// Copyright (C)'): L[1] = line
        else: L.insert(1, line)
        open(f, 'w', newline='\n', encoding='ascii').write('\n'.join(L)); n += 1
    print('intestazione Copyright aggiornata in %d file' % n)
