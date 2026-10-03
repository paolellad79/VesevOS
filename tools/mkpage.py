#!/usr/bin/env python3
# Genera VesevOS/vos_page.h dalla pagina web/index.html (solo ASCII).
# Uso: python3 tools/mkpage.py
import os
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
src = open(os.path.join(ROOT, 'web', 'index.html'), encoding='utf-8').read()
out = []
for ch in src:
    o = ord(ch)
    out.append(ch if o < 128 else '&#%d;' % o)
html = ''.join(out)
assert ')VOSPAGE"' not in html
# Il raw string literal ha limiti di lunghezza in alcuni compilatori: spezzo in blocchi concatenati
chunks = []
step = 8000
i = 0
while i < len(html):
    j = min(i + step, len(html))
    # non spezzare dentro una riga lunga: cerca newline
    k = html.rfind('\n', i, j) if j < len(html) else j
    if k <= i: k = j
    else: k = k + 1 if j < len(html) else j
    chunks.append(html[i:k]); i = k
body = '\n'.join('R"VOSPAGE(%s)VOSPAGE"' % c for c in chunks)
open(os.path.join(ROOT, 'VesevOS', 'vos_page.h'), 'w', encoding='ascii').write(
    '// VesevOS - vos_page.h (GENERATO, solo ASCII)\n#pragma once\n#include <Arduino.h>\nstatic const char INDEX_HTML[] PROGMEM =\n%s;\n' % body)
print('ok', len(html), 'byte,', len(chunks), 'blocchi')
