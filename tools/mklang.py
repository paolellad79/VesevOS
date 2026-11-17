#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# Genera i file di lingua lang/<codice>.vld e .vlp da tools/lang_src.json e controlla che tutto torni.
# Dentro il firmware c'e solo l'italiano (i testi del codice): tutte le altre lingue, inglese compreso, sono file in lang/
# da caricare nella scheda (guida iniziale o Lingue). Per ogni lingua: <codice>.vld (dizionario del firmware) e <codice>.vlp
# (frasi della pagina, gzip). I vecchi lang/<codice>.json non si generano piu (16/11/2026; la prova build/tests/lingue legge tools/lang_src.json). La bandiera italiana va in VesevOS/src/core/vos_lang_it.h (generato). Dalla 1.7.59.
# Uso: python3 tools/mklang.py        (esce con errore se qualcosa non va)
import os, re, sys, json, glob, gzip, struct
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import manuale_lib
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
UNICODE_FW = ('zh',)              # lingue il cui testo firmware non puo essere ASCII (cinese): solo nei file lang/<codice>.vld e .vlp, mai nel codice
BUILTIN = ()                      # dalla 1.7.59 nessuna lingua dentro il firmware oltre all'italiano

# ---- chiavi usate dal codice del firmware
src_text = ''
for f in sorted(glob.glob(os.path.join(FWD, '**', '*.cpp'), recursive=True) + glob.glob(os.path.join(FWD, '*.ino')) + glob.glob(os.path.join(FWD, 'src', '**', 'vos_shell_rep.h'), recursive=True)):
    src_text += open(f, encoding='ascii').read() + '\n'
# manuale dei comandi: le frasi delle voci BREVI stanno in tools/data/manuale_src.txt (non piu nel C++) e il firmware le cerca con tr()
for riga in manuale_lib.righe_brevi(manuale_lib.leggi()):
    src_text += '"%s"\n' % riga.replace('\\', '\\\\').replace('"', '\\"')
    if not all(ord(c) < 128 for c in riga): err('manuale_src.txt: riga non ASCII: %r' % riga)
tr_keys = set()
for m in re.finditer(r'\b(?:trf?|TRK)\(\s*"((?:[^"\\]|\\.)*)"', src_text):      # TRK = testo tradotto piu tardi (tabella dei comandi)
    tr_keys.add(m.group(1).encode().decode('unicode_escape'))
tr_keys.update(manuale_lib.righe_brevi(manuale_lib.leggi()))

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
for arr in ('TKCOLS', 'GRP', 'DAYS', 'LEDM', 'AIRO', 'UNITS', 'ONOFF', 'TKS', 'CKS', 'AKS', 'BN', 'BFIX'):
    for m in re.finditer(r'\["([^"]+)","([^"]+)"\]', re.search(r'var %s=.*?;' % arr, js, re.S).group(0)):
        addw(m.group(2))
for m in re.finditer(r'\{id:"[^"]+",testo:"([^"]+)"', re.search(r'var VOCI=\[.*?\];', js, re.S).group(0)):   # testi del menu
    addw(m.group(1))
for arr in ('SECN', 'SUSTEPS', 'MROLE', 'RNAME', 'WDD', 'STLW'):          # elenchi semplici di testi passati a t()
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
    kk = (k, e['scope'])          # la stessa frase puo stare una volta nel firmware e una nella pagina (es. 'Risorse')
    if kk in seen: err('duplicata: %r' % (kk,))
    seen.add(kk)
    for l in langs:
        if not e.get(l): err('manca %s per %r' % (l, k)); continue
        v = e[l]
        if e['scope'] == 'fw':
            if l not in UNICODE_FW and not all(ord(c) < 128 for c in v): err('non ASCII in %s (testo firmware): %r' % (l, v))
            if FMT.findall(k) != FMT.findall(v): err('segnaposto diversi in %s: %r' % (l, k))
        else:
            if sorted(BRC.findall(k)) != sorted(BRC.findall(v)): err('segnaposto {n} diversi in %s: %r' % (l, k))
    if e['scope'] == 'fw':
        if not all(ord(c) < 128 for c in k): err('chiave firmware non ASCII: %r' % k)

TESTO_MAX = 640                   # LANG_TESTO_MAX in core/vos_i18n_calc.h: la memoria circolare di tr() non tiene testi piu lunghi
for e in entries:
    if e['scope'] == 'fw':
        for l in langs:
            if e.get(l) and len(e[l].encode('utf-8')) > TESTO_MAX: err('testo firmware oltre %d byte in %s: %r' % (TESTO_MAX, l, e['it'][:60]))

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

# ---- formato dei file di lingua per scheda (vedi core/vos_i18n_calc.h)
def fnv1a(dati):
    h = 0x811c9dc5
    for c in dati:
        h ^= c
        h = (h * 0x01000193) & 0xffffffff
    return h

def make_vld(voci, nome, locale, bandiera):
    """voci = [(frase italiana, testo)]. Dizionario del firmware: tabella ordinata per impronta, testi non compressi."""
    ordinate = sorted((fnv1a(k.encode('utf-8')), k, t.encode('utf-8')) for k, t in voci)
    for a, b in zip(ordinate, ordinate[1:]):
        if a[0] == b[0]:
            print('ERRORE: due frasi hanno la stessa impronta %08x: %r e %r (cambia una delle due frasi)' % (a[0], a[1][:70], b[1][:70]))
            sys.exit(1)
    for riga in (nome, locale, bandiera):
        assert '\n' not in riga
    testata = ('nome=%s\nlocale=%s\nbandiera=%s\n' % (nome, locale, bandiera)).encode('utf-8')
    off_tab = 32 + len(testata)
    off_testi = off_tab + 10 * len(ordinate)
    tabella = bytearray()
    testi = bytearray()
    for h, k, t in ordinate:
        tabella += struct.pack('<IIH', h, len(testi), len(t))
        testi += t
    totale = off_testi + len(testi)
    intest = b'VOSL' + struct.pack('<BBHIIIIII', 1, 0, 0, len(ordinate), off_tab, off_testi, totale, 0, 0)
    assert len(intest) == 32
    return bytes(intest) + testata + bytes(tabella) + bytes(testi)

def make_vlp(voci):
    """Frasi della pagina: JSON piatto, una voce per riga, gzip -9 senza data (stesso risultato a ogni giro)."""
    txt = '{' + ',\n'.join(json.dumps(k, ensure_ascii=False) + ':' + json.dumps(v, ensure_ascii=False) for k, v in voci) + '}\n'
    return gzip.compress(txt.encode('utf-8'), 9, mtime=0)

# ---- generazione
os.makedirs(OUT, exist_ok=True)
def build(code, name):
    d = {'_name': name, '_locale': locales[code], '_flag': flags[code]}
    for e in entries:
        if e[code] != e['it']:        # se uguale all'italiano si risparmia spazio: manca = stesso testo
            d[e['it']] = e[code]
    return d
# bandiera italiana (l'italiano e la lingua del codice, sempre presente)
hit = ('// VesevOS - vos_lang_it.h (GENERATO da tools/mklang.py, non modificare a mano)\n#pragma once\n'
       'static const char LANG_FLAG_IT[] = %s;\n') % json.dumps(flags['it'], ensure_ascii=True)
open(os.path.join(FWD, 'src', 'core', 'vos_lang_it.h'), 'w', encoding='ascii', newline='\n').write(hit)
oldh = os.path.join(FWD, 'src', 'core', 'vos_lang_en.h')
if os.path.exists(oldh): os.remove(oldh)            # fino alla 1.7.58 l'inglese era dentro il firmware
for code, name in langs.items():
    if code == 'it': continue
    d = build(code, name)
    if code in BUILTIN:
        old = os.path.join(OUT, code + '.json')
        if os.path.exists(old): os.remove(old)       # e dentro il firmware: niente file
        txt = json.dumps(d, ensure_ascii=True, separators=(',', ':'))
        import gzip
        z = gzip.compress(txt.encode('ascii'), 9, mtime=0)      # compresso nel firmware (circa 28 KB invece di 100); si decomprime in PSRAM
        body = '\n'.join('  ' + ','.join('0x%02x' % c for c in z[k:k + 24]) + ',' for k in range(0, len(z), 24))
        def cstr(x): return json.dumps(x, ensure_ascii=True)
        h = ('// VesevOS - vos_lang_%s.h (GENERATO da tools/mklang.py, non modificare a mano)\n#pragma once\n#include "../drivers/vos_drv_string.h"\n'
             'static const char LANG_EN_NAME[] = %s;\nstatic const char LANG_EN_LOCALE[] = %s;\nstatic const char LANG_EN_FLAG[] = %s;\n'
             'static const char LANG_FLAG_IT[] = %s;\nstatic const size_t LANG_EN_GZ_LEN = %d;\nstatic const uint8_t LANG_EN_GZ[] PROGMEM = {\n%s\n};\n') % (
            code, cstr(name), cstr(locales[code]), cstr(flags[code]), cstr(flags['it']), len(z), body)
        assert all(ord(c) < 128 for c in h)
        open(os.path.join(FWD, 'src', 'core', 'vos_lang_%s.h' % code), 'w', encoding='ascii', newline='\n').write(h)
        print('VesevOS/src/core/vos_lang_%s.h: %d voci, %d byte (compressi: %d, dentro il firmware)' % (code, len(d) - 3, len(txt), len(z)))
        continue
    voci_fw = [(e['it'], e[code]) for e in entries if e['scope'] == 'fw' and e[code] != e['it']]
    voci_web = [(e['it'], e[code]) for e in entries if e['scope'] == 'web' and e[code] != e['it']]
    vld = make_vld(voci_fw, name, locales[code], flags[code])
    vlp = make_vlp(voci_web)
    open(os.path.join(OUT, code + '.vld'), 'wb').write(vld)
    open(os.path.join(OUT, code + '.vlp'), 'wb').write(vlp)
    print('lang/%s.vld: %d voci, %d byte; lang/%s.vlp: %d voci, %d byte (gzip)' % (code, len(voci_fw), len(vld), code, len(voci_web), len(vlp)))
print('ok: %d chiavi firmware nel codice, %d testi pagina' % (len(tr_keys), len(web_keys)))
