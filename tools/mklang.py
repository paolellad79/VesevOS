#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# Genera i file di lingua lang/<codice>.json da tools/lang_src.json e controlla che tutto torni.
# L'inglese va DENTRO il firmware: VesevOS/src/core/vos_lang_en.h (con la bandiera). Le altre lingue restano file (lang/).
# Uso: python3 tools/mklang.py        (esce con errore se qualcosa non va)
import os, re, sys, json, glob
from html.parser import HTMLParser

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC  = os.path.join(ROOT, 'tools', 'lang_src.json')
FWD  = os.path.join(ROOT, 'VesevOS')
HTML = os.path.join(ROOT, 'web', 'index.html')
OUT  = os.path.join(ROOT, 'lang')
errors = []
def err(m): errors.append(m)

data = json.load(open(SRC, encoding='utf-8'))
langs = data['langs']
entries = data['entries']
flags = data.get('flags', {})
locales = data.get('locales', {})
BUILTIN = ('en',)

# ---- chiavi usate dal codice del firmware
src_text = ''
for f in sorted(glob.glob(os.path.join(FWD, '**', '*.cpp'), recursive=True) + glob.glob(os.path.join(FWD, '*.ino'))):
    src_text += open(f, encoding='ascii').read() + '\n'
tr_keys = set()
for m in re.finditer(r'\btrf?\(\s*"((?:[^"\\]|\\.)*)"', src_text):
    tr_keys.add(m.group(1).encode().decode('unicode_escape'))

# ---- chiavi usate dalla pagina web
html = open(HTML, encoding='utf-8').read()
body = html[html.index('<body>'):]
static = re.sub(r'<script>.*</script>', '', body, flags=re.S)
js = re.search(r'<script>(.*)</script>', body, re.S).group(1)
web_keys = set()
def addw(k):
    k = re.sub(r'\s+', ' ', k).strip()
    if k: web_keys.add(k)
class P(HTMLParser):
    def __init__(self): super().__init__(convert_charrefs=True); self.skip = False
    def handle_starttag(self, tag, attrs):
        if tag in ('script', 'style'): self.skip = True
        tt = any(k == 'data-tt' for k, v in attrs)
        for k, v in attrs:
            if k == 'placeholder' and v: addw(v)
            if tt and k in ('title', 'aria-label') and v: addw(v)
    def handle_endtag(self, tag):
        if tag in ('script', 'style'): self.skip = False
    def handle_data(self, d):
        if not self.skip: addw(d)
P().feed(static)
for m in re.finditer(r'\b(?:window\.)?tf?\(\s*"((?:[^"\\]|\\.)*)"', js):
    addw(json.loads('"' + m.group(1) + '"'))
for arr in ('TABS', 'RSUB', 'TKCOLS', 'GRP', 'DAYS', 'LEDM', 'AIRO', 'UNITS', 'ONOFF', 'TKS', 'CKS', 'AKS', 'BN', 'BFIX'):
    for m in re.finditer(r'\["([^"]+)","([^"]+)"\]', re.search(r'var %s=.*?;' % arr, js, re.S).group(0)):
        addw(m.group(2))
for arr in ('SUSTEPS', 'MROLE', 'RNAME', 'WDD', 'STLW'):          # elenchi semplici di testi passati a t()
    for m in re.finditer(r'"([^"]+)"', re.search(r'var %s=\[.*?\];' % arr, js, re.S).group(0)):
        addw(m.group(1))
for m in re.finditer(r'var APLAW="((?:[^"\\]|\\.)*)"', js): addw(json.loads('"' + m.group(1) + '"'))
web_keys.discard('VesevOS')

# ---- controlli sulle voci
FMT = re.compile(r'%[-+ #0]*\d*(?:\.\d+)?(?:l|h)?[sduxXcf%]')
BRC = re.compile(r'\{\d\}')
seen = set()
for e in entries:
    k = e['it']
    if k in seen: err('duplicata: %r' % k)
    seen.add(k)
    for l in langs:
        if not e.get(l): err('manca %s per %r' % (l, k)); continue
        v = e[l]
        if e['scope'] == 'fw':
            if not all(ord(c) < 128 for c in v): err('non ASCII in %s (testo firmware): %r' % (l, v))
            if FMT.findall(k) != FMT.findall(v): err('segnaposto diversi in %s: %r' % (l, k))
        else:
            if sorted(BRC.findall(k)) != sorted(BRC.findall(v)): err('segnaposto {n} diversi in %s: %r' % (l, k))
    if e['scope'] == 'fw':
        if not all(ord(c) < 128 for c in k): err('chiave firmware non ASCII: %r' % k)

fwkeys = {e['it'] for e in entries if e['scope'] == 'fw'}
webkeys = {e['it'] for e in entries}
for k in sorted(tr_keys):
    if k not in fwkeys: err('nel codice tr("%s") ma manca in lang_src.json (scope fw)' % k)
for k in sorted(fwkeys):
    if '"' + k.replace('\\', '\\\\').replace('"', '\\"') + '"' not in src_text: err('voce fw non usata nel codice: %r' % k)
for k in sorted(web_keys):
    if k not in webkeys: err('testo della pagina senza traduzione: %r' % k)
for e in entries:
    if e['scope'] == 'web' and e['it'] not in web_keys:
        err('voce web non usata nella pagina: %r' % e['it'])

for code in ['it'] + list(langs):
    f = flags.get(code, '')
    if not f.startswith('<svg') or 'xmlns=' not in f: err('bandiera mancante o non valida: %s' % code)
    elif len(f.encode('utf-8')) > 1024: err('bandiera troppo grande (max 1 KB): %s' % code)
    if '<script' in f.lower() or ' on' in f.lower(): err('bandiera con script o eventi: %s' % code)
    if not locales.get(code): err('codice locale mancante: %s' % code)

if errors:
    print('ERRORI (%d):' % len(errors))
    for x in errors: print(' -', x)
    sys.exit(1)

# ---- generazione
os.makedirs(OUT, exist_ok=True)
def build(code, name):
    d = {'_name': name, '_locale': locales[code], '_flag': flags[code]}
    for e in entries:
        if e[code] != e['it']:        # se uguale all'italiano si risparmia spazio: manca = stesso testo
            d[e['it']] = e[code]
    return d
for code, name in langs.items():
    d = build(code, name)
    if code in BUILTIN:
        old = os.path.join(OUT, code + '.json')
        if os.path.exists(old): os.remove(old)       # e dentro il firmware: niente file
        txt = json.dumps(d, ensure_ascii=True, separators=(',', ':'))
        import gzip
        z = gzip.compress(txt.encode('ascii'), 9, mtime=0)      # compresso nel firmware (circa 28 KB invece di 100); si decomprime in PSRAM
        body = '\n'.join('  ' + ','.join('0x%02x' % c for c in z[k:k + 24]) + ',' for k in range(0, len(z), 24))
        def cstr(x): return json.dumps(x, ensure_ascii=True)
        h = ('// VesevOS - vos_lang_%s.h (GENERATO da tools/mklang.py, non modificare a mano)\n#pragma once\n#include <Arduino.h>\n'
             'static const char LANG_EN_NAME[] = %s;\nstatic const char LANG_EN_LOCALE[] = %s;\nstatic const char LANG_EN_FLAG[] = %s;\n'
             'static const char LANG_FLAG_IT[] = %s;\nstatic const size_t LANG_EN_GZ_LEN = %d;\nstatic const uint8_t LANG_EN_GZ[] PROGMEM = {\n%s\n};\n') % (
            code, cstr(name), cstr(locales[code]), cstr(flags[code]), cstr(flags['it']), len(z), body)
        assert all(ord(c) < 128 for c in h)
        open(os.path.join(FWD, 'src', 'core', 'vos_lang_%s.h' % code), 'w', encoding='ascii', newline='\n').write(h)
        print('VesevOS/src/core/vos_lang_%s.h: %d voci, %d byte (compressi: %d, dentro il firmware)' % (code, len(d) - 3, len(txt), len(z)))
        continue
    txt = json.dumps(d, ensure_ascii=False, separators=(',', ':'), indent=None)
    txt = txt.replace('","', '",\n"')  # una voce per riga: file leggibile e confrontabile
    open(os.path.join(OUT, code + '.json'), 'w', encoding='utf-8', newline='\n').write(txt + '\n')
    print('lang/%s.json: %d voci, %d byte' % (code, len(d) - 3, len(txt.encode('utf-8'))))
print('ok: %d chiavi firmware nel codice, %d testi pagina' % (len(tr_keys), len(web_keys)))
