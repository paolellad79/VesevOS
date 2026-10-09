#!/usr/bin/env python3
# Genera VesevOS/src/interfaces/vos_page.h dalla pagina web/index.html: pagina COMPRESSA (gzip) in un array di byte.
# Il browser la riceve con "Content-Encoding: gzip". Il file .h contiene solo numeri (ASCII):
# i caratteri speciali della pagina (accenti, simboli) restano UTF-8 veri, senza conversioni in &#...;
# Uso: python3 tools/mkpage.py
import os, gzip
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
raw = open(os.path.join(ROOT, 'web', 'index.html'), encoding='utf-8').read().encode('utf-8')
gz = gzip.compress(raw, 9, mtime=0)
lines = [','.join('0x%02x' % b for b in gz[i:i + 24]) for i in range(0, len(gz), 24)]
h = ('// VesevOS - vos_page.h (GENERATO da tools/mkpage.py, non modificare a mano)\n#pragma once\n#include <Arduino.h>\n'
     'static const size_t INDEX_GZ_LEN = %d;\nstatic const uint8_t INDEX_GZ[] PROGMEM = {\n%s\n};\n') % (len(gz), ',\n'.join(lines))
open(os.path.join(ROOT, 'VesevOS', 'src', 'interfaces', 'vos_page.h'), 'w', encoding='ascii', newline='\n').write(h)
print('ok pagina: %d byte, compressa %d byte' % (len(raw), len(gz)))
