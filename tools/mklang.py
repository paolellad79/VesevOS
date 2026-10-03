#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# Genera i file di lingua lang/<codice>.json da tools/lang_src.json e controlla che tutto torni.
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

# ---- chiavi usate dal codice del firmware
src_text = ''
for f in sorted(glob.glob(os.path.join(FWD, '*.cpp')) + glob.glob(os.path.join(FWD, '*.ino'))):
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
        for k, v in attrs:
            if k == 'placeholder' and v: addw(v)
    def handle_endtag(self, tag):
        if tag in ('script', 'style'): self.skip = False
    def handle_data(self, d):
        if not self.skip: addw(d)
P().feed(static)
for m in re.finditer(r'\b(?:window\.)?tf?\(\s*"((?:[^"\\]|\\.)*)"', js):
    addw(json.loads('"' + m.group(1) + '"'))
for arr in ('TABS', 'RSUB', 'TKCOLS', 'GRP', 'DAYS', 'LEDM', 'LED2', 'UNITS', 'ONOFF', 'TKS', 'CKS', 'AKS', 'BN', 'BFIX'):
    for m in re.finditer(r'\["([^"]+)","([^"]+)"\]', re.search(r'var %s=.*?;' % arr, js, re.S).group(0)):
        addw(m.group(2))
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

if errors:
    print('ERRORI (%d):' % len(errors))
    for x in errors: print(' -', x)
    sys.exit(1)

# ---- generazione
os.makedirs(OUT, exist_ok=True)
for code, name in langs.items():
    d = {'_name': name}
    for e in entries:
        if e[code] != e['it']:        # se uguale all'italiano si risparmia spazio: manca = stesso testo
            d[e['it']] = e[code]
    txt = json.dumps(d, ensure_ascii=False, separators=(',', ':'), indent=None)
    txt = txt.replace('","', '",\n"')  # una voce per riga: file leggibile e confrontabile
    open(os.path.join(OUT, code + '.json'), 'w', encoding='utf-8', newline='\n').write(txt + '\n')
    print('lang/%s.json: %d voci, %d byte' % (code, len(d) - 1, len(txt.encode('utf-8'))))
print('ok: %d chiavi firmware nel codice, %d testi pagina' % (len(tr_keys), len(web_keys)))
