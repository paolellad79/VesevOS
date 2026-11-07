#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# Traduce con Ollama locale le voci di tools/lang_src.json in una nuova lingua (bozza automatica, da far rivedere a madrelingua).
# Uso: python3 tools/traduci_ollama.py <codice> <web|fw|all> [modello] [dimensione-blocco] [limite]
# Il risultato (it -> testo) sta in tools/<codice>_cache.json; si riprende da dove si era arrivati. Controlli automatici per ogni voce:
# segnaposto uguali, tag uguali, a capo uguali, nessuna parola inglese/italiana rimasta per errore. Le voci che non passano restano
# fuori dalla cache e vanno in tools/<codice>_da_rivedere.json.
import json, os, re, sys, time, urllib.request

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
code, scope = sys.argv[1], sys.argv[2]
model = sys.argv[3] if len(sys.argv) > 3 else 'qwen3.5:9b'
bsize = int(sys.argv[4]) if len(sys.argv) > 4 else 25
limit = int(sys.argv[5]) if len(sys.argv) > 5 else 10**9
CACHE = os.path.join(ROOT, 'tools', code + '_cache.json')
REV = os.path.join(ROOT, 'tools', code + '_da_rivedere.json')
NAMES = {'zh': 'Simplified Chinese (zh-CN)'}
FMT = re.compile(r'%[-+ #0]*\d*(?:\.\d+)?(?:l|h)?[sduxXcf%]')
BRC = re.compile(r'\{\d\}')
TAG = re.compile(r'<[^>]+>')
LAT = re.compile(r'[A-Za-z]{3,}')

data = json.load(open(os.path.join(ROOT, 'tools', 'lang_src.json'), encoding='utf-8'))
gloss = json.load(open(os.path.join(ROOT, 'tools', 'glossario_' + code + '.json'), encoding='utf-8'))
cache = json.load(open(CACHE, encoding='utf-8')) if os.path.exists(CACHE) else {}
bad = json.load(open(REV, encoding='utf-8')) if os.path.exists(REV) else {}

SYSTEM = ('You translate UI texts of an embedded firmware (ESP32 web page and command line) from Italian into %s. Technical, neutral tone. '
 'Rules: keep EXACTLY the same placeholders ({0} {1} %%s %%d %%u %%lu %%x %%-24s etc.), the same HTML tags, the same line breaks, '
 'shell commands, file names, paths, option names and numbers. Do not translate the product and technical names: VesevOS, ESP32, Wi-Fi, BLE, MQTT, ELF, Berry, SHA-256, GPIO, I2C, SPI, microSD, FAT32, LED, USB, TLS, HTTP, NTP, mDNS. '
 'Use this glossary (Italian -> Chinese): %s. '
 'Input is a JSON array of Italian strings. Output ONLY a JSON array of the same length with the translations, in the same order. No explanations.'
 % (NAMES[code], json.dumps(gloss, ensure_ascii=False)))

def ask(strings):
    body = {'model': model, 'stream': False, 'think': False, 'options': {'temperature': 0, 'num_predict': 6000},
            'format': {'type': 'array', 'items': {'type': 'string'}},
            'messages': [{'role': 'system', 'content': SYSTEM}, {'role': 'user', 'content': json.dumps(strings, ensure_ascii=False)}]}
    req = urllib.request.Request('http://localhost:11434/api/chat', data=json.dumps(body).encode(), headers={'Content-Type': 'application/json'})
    r = json.load(urllib.request.urlopen(req, timeout=3000))
    return json.loads(r['message']['content'])

def valid(src, out, fw):
    if not isinstance(out, str) or not out.strip(): return 'vuota'
    if '```' in out: return 'markdown'
    if fw:
        if FMT.findall(src) != FMT.findall(out): return 'segnaposto %'
    elif sorted(BRC.findall(src)) != sorted(BRC.findall(out)): return 'segnaposto {n}'
    if sorted(TAG.findall(src)) != sorted(TAG.findall(out)): return 'tag'
    if src.count('\n') != out.count('\n'): return 'a capo'
    words = [w for w in LAT.findall(re.sub(r'\{\d\}|%[-0-9.]*[a-z]', '', src)) if len(w) >= 4]
    if words and not re.search(r'[一-鿿]', out): return 'senza cinese'
    extra = [w for w in LAT.findall(out) if w.lower() not in src.lower() and w.lower() not in ('http', 'https')]
    if extra: return 'parole latine nuove: ' + ','.join(extra[:3])
    return None

def need(e):
    return (scope == 'all' or (scope == 'fw') == (e['scope'] == 'fw')) and e['it'] not in cache and LAT.search(re.sub(r'\{\d\}|%[-0-9.]*[a-z]', '', e['it']))

todo = [e for e in data['entries'] if need(e)][:limit]
print('da tradurre', len(todo), 'modello', model, flush=True)
t0 = time.time(); done = 0
def run(items, tries):
    global done
    try: out = ask([e['it'] for e in items])
    except Exception as ex:
        out = None; print('ERR', ex, flush=True)
    if not out or len(out) != len(items):
        if len(items) > 1:
            h = len(items) // 2
            run(items[:h], tries); run(items[h:], tries); return
        bad[items[0]['it']] = 'risposta non valida'; return
    for e, o in zip(items, out):
        why = valid(e['it'], o, e['scope'] == 'fw')
        if why is None: cache[e['it']] = o.strip() if '\n' not in e['it'] else o; bad.pop(e['it'], None)
        elif tries > 0 and len(items) > 1: continue
        else: bad[e['it']] = why + ' | ' + str(o)[:120]
    redo = [e for e, o in zip(items, out) if e['it'] not in cache and e['it'] not in bad]
    if redo and tries > 0: run(redo, tries - 1)
    for e in redo:
        if e['it'] not in cache and e['it'] not in bad: bad[e['it']] = 'non passata'
for i in range(0, len(todo), bsize):
    run(todo[i:i + bsize], 2)
    done += len(todo[i:i + bsize])
    json.dump(cache, open(CACHE, 'w', encoding='utf-8'), ensure_ascii=False, indent=0)
    json.dump(bad, open(REV, 'w', encoding='utf-8'), ensure_ascii=False, indent=0)
    el = time.time() - t0
    print('%d/%d  ok=%d da_rivedere=%d  %ds  stima restante %ds' % (done, len(todo), len(cache), len(bad), el, el / done * (len(todo) - done)), flush=True)
