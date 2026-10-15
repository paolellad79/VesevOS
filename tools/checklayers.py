#!/usr/bin/env python3
# VesevOS - controllo degli strati: l'hardware si tocca SOLO nei driver (claude/VesevOS-schema-strati.md).
# Per ogni famiglia di chiamate hardware conta le righe di codice (commenti e testi tra virgolette esclusi) fuori dal driver.
# Seconda parte: la DIPENDENZA DA ARDUINO (String, Serial, millis/micros/delay, Arduino.h). Oggi e ammessa solo nei driver
# (vos_drv_*) e in third_party/; il resto e debito in baseline. Quando ci sara un file di compatibilita, si aggiunge qui ad ARDUINO_OK.
# Il "debito" di oggi sta in tools/layers_baseline.json: puo solo calare. Un file nuovo o un conteggio piu alto = errore.
# Uso:  python3 tools/checklayers.py            (controlla)
#       python3 tools/checklayers.py --update   (riscrive il debito dopo aver tolto accessi; mai per aumentarlo)
import json, os, re, sys
HERE = os.path.dirname(os.path.abspath(__file__)); SRC = os.path.join(HERE, '..', 'VesevOS')
BASE = os.path.join(HERE, 'layers_baseline.json')
# famiglia -> (espressione, file driver che possono usarla; vos_license_data.h e solo testo di licenze)
RULES = {
  'ble':    (r'\bNimBLE\w*|\bBLEDevice\b',                    {'vos_drv_ble.cpp', 'vos_drv_ble.h', 'vos_license_data.h'}),
  'espnow': (r'\besp_now_\w+',                                 {'vos_drv_espnow.cpp', 'vos_drv_espnow.h'}),
  'mqtt':   (r'\besp_mqtt_client_\w+|\bmqtt_client\.h',        {'vos_drv_mqtt.cpp', 'vos_drv_mqtt.h'}),
  'wifi':   (r'\bWiFi\.|\besp_wifi_\w+',                       {'vos_drv_espnow.cpp', 'vos_drv_wifi.cpp', 'vos_drv_wifi.h', 'vos_drv_wps.cpp'}),     # il driver ESP-NOW legge solo modo e canale
  'fs':     (r'\bLittleFS\b(?!\.h)',                           {'vos_drv_fs.cpp', 'vos_drv_fs.h', 'vos_license_data.h'}),
  'ota':    (r'\besp_partition_\w+|\besp_ota_\w+',                {'vos_drv_ota.cpp', 'vos_drv_ota.h'}),
  'i2c':    (r'\bWire\.|\bTwoWire\b|\bWire\.h',               {'vos_drv_i2c.cpp', 'vos_drv_i2c.h'}),
  'gpio':   (r'\b(digitalWrite|digitalRead|pinMode|analogRead|ledcWrite|ledcAttach|neopixelWrite)\s*\(', {'vos_drv_gpio.cpp'}),
  'nvs':    (r'\bPreferences\b|\bnvs_(?:open|get|set|commit|erase)\w*',  {'vos_drv_nvs.h', 'vos_drv_nvs.cpp'}),          # NVS: solo DrvNvs
  'restart': (r'\bESP\.restart\b|\besp_restart\s*\(',              {'vos_drv_sys.cpp'}),                             # riavvio: solo sysReboot -> driver
}
# STRATI: core, devices, net, security, drivers e boards NON includono packages/ o interfaces/ (usano core/vos_hooks.h e il
# registro dei servizi). Nessun debito ammesso: deve restare 0. Solo VesevOS.ino e src/vos_wire.cpp collegano tutto.
LOW = ('core', 'devices', 'net', 'security', 'drivers', 'boards')
INC_UP = re.compile(r'^\s*#\s*include\s*"(?:\.\./)*(?:packages|interfaces)/')
# ORDINE dei livelli (docs/progetto/REGOLE-ARCHITETTURA.md): 0 boards, 1 drivers, 2 core, 3 security, 4 devices, 5 net, 6 packages, 7 interfaces.
# TABELLA delle coppie CONSENTITE "cartella -> cartelle che puo includere" (derivata dalle dipendenze vere di oggi, senza le violazioni).
# Ogni altra coppia (verso l'alto, ma anche verso il basso se non e in tabella) e debito: coppia "da->a" in baseline, puo solo scendere;
# una coppia nuova = errore. Per aprire una coppia nuova si cambia la tabella QUI e in REGOLE-ARCHITETTURA.md, di proposito.
# Fuori scala: third_party/ (lo usano tutti), src/vos_wire.* e VesevOS.ino (livello 8, collegano tutto).
LEVEL = {'boards': 0, 'drivers': 1, 'core': 2, 'security': 3, 'devices': 4, 'net': 5, 'packages': 6, 'interfaces': 7}
ALLOWED = {
  'boards':     set(),
  'drivers':    {'boards'},
  'core':       {'boards', 'drivers'},
  'security':   {'core', 'drivers'},
  'devices':    {'core', 'drivers'},
  'net':        {'core', 'drivers', 'devices'},
  'packages':   {'core', 'drivers', 'security', 'devices', 'net'},
  'interfaces': {'core', 'drivers', 'security', 'devices', 'net', 'packages'},
}
INC_DIR = re.compile(r'^\s*#\s*include\s*"(?:\.\./)*(\w+)/')
# NUOVI CONTROLLI (15/10/2026), tutti con debito di partenza in baseline (puo solo calare):
#  e_idf  = include di ESP-IDF (esp_*.h, driver/*.h) fuori da drivers/, boards/ e third_party/: l'IDF si tocca solo dai driver
#  f_rtos = primitive FreeRTOS (xTaskCreate*, xQueueCreate*, xSemaphoreCreate*, xEventGroupCreate*, vTaskDelay*) fuori da drivers/ e core/
#  cicli  = gruppi di moduli (coppia .h/.cpp con lo stesso nome = un modulo) che si includono a vicenda; un gruppo nuovo o piu grande = errore
#  cicli_cart = lo stesso tra cartelle (la tabella ALLOWED e un grafo senza cicli: deve restare 0)
INC_IDF = re.compile(r'^\s*#\s*include\s*[<"](?:esp_[\w/]*\.h|driver/[\w/]*\.h)[>"]')
RTOS = re.compile(r'(?<![\w.>])(?:xTaskCreate\w*|xQueueCreate\w*|xSemaphoreCreate\w*|xEventGroupCreate\w*|vTaskDelay\w*)\s*\(')
INC_ANY = re.compile(r'^\s*#\s*include\s*"([^"]+)"')
IDF_OK_DIRS = ('drivers', 'boards', 'third_party')
RTOS_OK_DIRS = ('drivers', 'core', 'third_party', 'boards')
def sccs(graph):
    """Componenti fortemente connesse (Tarjan) con piu di un nodo."""
    idx, low, onst, st, out, cnt = {}, {}, set(), [], [], [0]
    def go(v):
        idx[v] = low[v] = cnt[0]; cnt[0] += 1; st.append(v); onst.add(v)
        for w in sorted(graph.get(v, ())):
            if w not in idx: go(w); low[v] = min(low[v], low[w])
            elif w in onst: low[v] = min(low[v], idx[w])
        if low[v] == idx[v]:
            comp = []
            while True:
                w = st.pop(); onst.discard(w); comp.append(w)
                if w == v: break
            if len(comp) > 1: out.append(sorted(comp))
    sys.setrecursionlimit(10000)
    for v in sorted(graph):
        if v not in idx: go(v)
    return out
# famiglie "Arduino": (espressione, usa il testo grezzo con le virgolette?)  - ammessi: ARDUINO_OK
ARDUINO = {
  'a_string': (r'(?<![\w:])String\b',                                      False),   # tipo String di Arduino (non std::string, WString, StringBuilder)
  'a_serial': (r'(?<![\w.>])(?:Serial[0-9]?|USBSerial|HWCDC)\b',             False),
  'a_time':   (r'(?<![\w.>:])(?:millis|micros|delay|delayMicroseconds)\s*\(', False),
  'a_include': (r'^\s*#\s*include\s*[<"]Arduino\.h[>"]',                    True),
}
# famiglia "scheda": pin, limiti dei GPIO, chip e partizioni scritti a mano. Devono stare nel file di scheda (VesevOS/src/boards/).
# Ammessi: boards/, third_party/ e i file generati (pagina, licenze, dati comuni). Testo grezzo senza i commenti di riga.
SCHEDA = {
  's_pin':   r'\b(?:pinMode|digitalWrite|digitalRead|analogRead|neopixelWrite|rgbLedWrite|pinClaim|drvGpio\w+)\s*\(\s*\d',   # numero di pin scritto nella chiamata
  's_limit': r'(?:<=|==|!=)\s*48\b|<\s*49\b|\[\s*49\s*\]|\bgpio\w*\s*(?:<=|>=|<|>)\s*\d{2}\b|\bg\s*(?:<=|>=)\s*(?:21|33)\b',   # limiti dei GPIO di questa scheda
  's_chip':  r'ESP32-S3|SuperMini|\besp32s3\b|\bESP32S3\b|CONFIG_IDF_TARGET_ESP32S3',   # chip o scheda nominati
  's_mem':   r'0x110000|2424832|\b(?:4|2)\s*MB\s*(?:flash|PSRAM)|\bpsramFound\b|\bgetPsramSize\b',   # flash, PSRAM e partizioni
  's_sys':   r'\bsetCpuFrequencyMhz\b|\btemperatureRead\b|\btemp_sensor\w*|\brgbLedWrite\b',   # CPU, temperatura e LED WS2812
}
SCHEDA_GEN = {'vos_license_data.h', 'vos_common_data.h', 'vos_lang_data.h'}
def SCHEDA_OK(f, path):
    return (os.sep + 'boards' + os.sep in path or os.sep + 'third_party' + os.sep in path or f in SCHEDA_GEN or (f.startswith('vos_page') and f.endswith('.h')))
def ARDUINO_OK(f, path):
    return f.startswith('vos_drv_') or os.sep + 'third_party' + os.sep in path
SHINT = {'s_pin': 'numero di pin nella chiamata', 's_limit': 'limite dei GPIO', 's_chip': 'nome del chip/scheda', 's_mem': 'flash/PSRAM/partizioni', 's_sys': 'CPU/temperatura/LED'}
HINT = {'a_string': 'tipo String', 'a_serial': 'Serial', 'a_time': 'millis/micros/delay', 'a_include': 'include di Arduino.h'}
def code_only(line):
    line = re.sub(r'"(\\.|[^"\\])*"', '""', line)          # testi
    line = re.sub(r"'(\\.|[^'\\])'", "''", line)            # caratteri
    line = re.sub(r'//.*', '', line)                        # commento di riga
    return line
def scan():
    out = {}
    allf = []
    for root, _, fs in os.walk(SRC):
        for f in fs:
            if f.endswith(('.cpp', '.h', '.ino')): allf.append((f, os.path.join(root, f)))
    edges, stems = {}, {}
    for f, path in sorted(allf):
        if os.sep + 'third_party' + os.sep not in path: stems[f] = os.path.splitext(f)[0]
    for f, path in sorted(allf):
        rel0 = os.path.relpath(path, os.path.join(SRC, 'src')); top = rel0.split(os.sep)[0] if os.sep in rel0 and not rel0.startswith('..') else ''
        inblock = False
        for line in open(path, encoding='utf-8', errors='replace'):
            if inblock:
                if '*/' in line: inblock = False; line = line.split('*/', 1)[1]
                else: continue
            line = re.sub(r'/\*.*?\*/', ' ', line)                      # commento di blocco sulla stessa riga
            nc = code_only(line)                                    # "/*" in un commento di riga (es. /apps/*.be) o in un testo non conta
            if '/*' in nc and '*/' not in nc.split('/*', 1)[1]: inblock = True; line = nc.split('/*', 1)[0]
            c = code_only(line)
            if INC_UP.match(line) and os.path.relpath(path, os.path.join(SRC, 'src')).split(os.sep)[0] in LOW:
                out.setdefault('strati', {}); out['strati'][f] = out['strati'].get(f, 0) + 1
            m = INC_DIR.match(line)
            me = os.path.relpath(path, os.path.join(SRC, 'src')).split(os.sep)[0]
            if m and me in LEVEL and m.group(1) in LEVEL and m.group(1) != me and m.group(1) not in ALLOWED[me]:
                out.setdefault('ordine', {}); k = me + '->' + m.group(1); out['ordine'][k] = out['ordine'].get(k, 0) + 1
            if INC_IDF.match(line) and top not in IDF_OK_DIRS:
                out.setdefault('e_idf', {}); out['e_idf'][f] = out['e_idf'].get(f, 0) + 1
            n = len(RTOS.findall(c)) if top not in RTOS_OK_DIRS else 0
            if n: out.setdefault('f_rtos', {}); out['f_rtos'][f] = out['f_rtos'].get(f, 0) + n
            mi = INC_ANY.match(line)
            if mi and f in stems and os.path.basename(mi.group(1)) in stems and stems[os.path.basename(mi.group(1))] != stems[f]:
                edges.setdefault(stems[f], set()).add(stems[os.path.basename(mi.group(1))])
                tf, tt = top, next((os.path.relpath(p, os.path.join(SRC, 'src')).split(os.sep)[0] for g, p in allf if g == os.path.basename(mi.group(1))), '')
                if tf in LEVEL and tt in LEVEL and tf != tt: out.setdefault('_cart', {}).setdefault(tf, set()).add(tt)
            for fam, (rx, ok) in RULES.items():
                if f in ok: continue
                n = len(re.findall(rx, c))
                if n: out.setdefault(fam, {}); out[fam][f] = out[fam].get(f, 0) + n
            raw = re.sub(r'//.*', '', line)
            if not SCHEDA_OK(f, path):
                for fam, rx in SCHEDA.items():
                    n = len(re.findall(rx, raw))
                    if n: out.setdefault(fam, {}); out[fam][f] = out[fam].get(f, 0) + n
            if ARDUINO_OK(f, path): continue
            for fam, (rx, usa_raw) in ARDUINO.items():
                n = len(re.findall(rx, raw if usa_raw else c))
                if n: out.setdefault(fam, {}); out[fam][f] = out[fam].get(f, 0) + n
    out['cicli'] = {','.join(c): 1 for c in sccs(edges)}
    out['cicli_cart'] = {','.join(c): 1 for c in sccs({k: v for k, v in out.pop('_cart', {}).items()})}
    for k in ('cicli', 'cicli_cart'):
        if not out[k]: del out[k]
    return out
cur = scan()
if '--update' in sys.argv:
    json.dump(cur, open(BASE, 'w'), indent=1, sort_keys=True); print('debito riscritto:', {k: sum(v.values()) for k, v in cur.items()}); sys.exit(0)
base = json.load(open(BASE)) if os.path.exists(BASE) else {}
bad = 0
for fam in RULES:
    for f, n in cur.get(fam, {}).items():
        b = base.get(fam, {}).get(f, 0)
        if n > b: print(f'KO {fam}: {f} tocca l\'hardware {n} volte (ammesso {b}). Usa il driver.'); bad += 1
for f, n in cur.get('strati', {}).items():
    print(f'KO strati: {f} include packages/ o interfaces/ {n} volte. Lo strato basso usa core/vos_hooks.h o il registro dei servizi; il collegamento sta in src/vos_wire.cpp.'); bad += 1
for k, n in sorted(cur.get('ordine', {}).items()):
    b = base.get('ordine', {}).get(k, 0)
    if n > b: print(f'KO ordine: {k} coppia non in tabella (ALLOWED) {n} volte (ammesso {b}). Verso l alto usa core/vos_hooks.h o il registro dei servizi (il collegamento sta in src/vos_wire.cpp); una apertura voluta si scrive nella tabella e in REGOLE-ARCHITETTURA.md.'); bad += 1
for f, n in cur.get('e_idf', {}).items():
    b = base.get('e_idf', {}).get(f, 0)
    if n > b: print(f'KO e_idf: {f} include {n} header di ESP-IDF (esp_*.h, driver/*.h) fuori dai driver (ammesso {b}). Metti la chiamata in un driver (vos_drv_*).'); bad += 1
for f, n in cur.get('f_rtos', {}).items():
    b = base.get('f_rtos', {}).get(f, 0)
    if n > b: print(f'KO f_rtos: {f} usa primitive FreeRTOS {n} volte fuori da drivers/ e core/ (ammesso {b}). Task, code e semafori si creano nel driver o nel nucleo.'); bad += 1
for fam, nome in (('cicli', 'moduli'), ('cicli_cart', 'cartelle')):
    bc = [set(k.split(',')) for k in base.get(fam, {})]
    for k in cur.get(fam, {}):
        if not any(set(k.split(',')) <= b for b in bc): print(f'KO {fam}: ciclo tra {nome} nuovo o piu grande: {k.replace(",", " <-> ")}. Si rompe con un gancio (core/vos_hooks.h) o spostando il pezzo comune sotto.'); bad += 1
for fam in ARDUINO:
    for f, n in cur.get(fam, {}).items():
        b = base.get(fam, {}).get(f, 0)
        if n > b: print(f'KO {fam}: {f} usa Arduino ({HINT[fam]}) {n} volte (ammesso {b}). Non aggiungere dipendenze da Arduino fuori dai driver: metti il codice nuovo in un driver (vos_drv_*) o senza Arduino.'); bad += 1
for fam in SCHEDA:
    for f, n in cur.get(fam, {}).items():
        b = base.get(fam, {}).get(f, 0)
        if n > b: print(f'KO {fam}: {f} ha {n} valori della scheda scritti a mano ({SHINT[fam]}; ammessi {b}). Mettili nel file di scheda (VesevOS/src/boards/).'); bad += 1
tot = {fam: sum(cur.get(fam, {}).values()) for fam in list(RULES) + list(ARDUINO) + list(SCHEDA)}
old = {fam: sum(base.get(fam, {}).values()) for fam in list(RULES) + list(ARDUINO) + list(SCHEDA)}
print('accessi hardware fuori dai driver (ora / debito di partenza):', ', '.join(f'{k} {tot[k]}/{old[k]}' for k in RULES))
print('include verso strati alti da core/devices/net/security/drivers/boards:', sum(cur.get('strati', {}).values()), '(deve essere 0)')
print('ordine dei livelli, coppie fuori tabella ALLOWED (ora / debito di partenza):', sum(cur.get('ordine', {}).values()), '/', sum(base.get('ordine', {}).values()), '-', ', '.join(f'{k} {n}/{base.get("ordine", {}).get(k, 0)}' for k, n in sorted(cur.get('ordine', {}).items())))
print('include di ESP-IDF fuori dai driver (ora / debito di partenza):', sum(cur.get('e_idf', {}).values()), '/', sum(base.get('e_idf', {}).values()), 'in', len(cur.get('e_idf', {})), 'file;', 'primitive FreeRTOS fuori da drivers/ e core/:', sum(cur.get('f_rtos', {}).values()), '/', sum(base.get('f_rtos', {}).values()), 'in', len(cur.get('f_rtos', {})), 'file')
print('cicli tra moduli (gruppi, ora / debito di partenza):', len(cur.get('cicli', {})), '/', len(base.get('cicli', {})), ' cicli tra cartelle:', len(cur.get('cicli_cart', {})), '(deve essere 0)')
for k in cur.get('cicli', {}): print('  ciclo:', k.replace(',', ' <-> '))
print('dipendenza da Arduino fuori dai driver (ora / debito di partenza):', ', '.join(f'{k[2:]} {tot[k]}/{old[k]}' for k in ARDUINO))
print('valori della scheda scritti a mano fuori da boards/ (ora / debito di partenza):', ', '.join(f'{k[2:]} {tot[k]}/{old[k]}' for k in SCHEDA))
for fam in SCHEDA:
    peggio = sorted(cur.get(fam, {}).items(), key=lambda x: -x[1])[:10]
    print(f'  {fam[2:]} ({len(cur.get(fam, {}))} file): ' + ', '.join(f'{f} {n}' for f, n in peggio))
for fam in ARDUINO:
    peggio = sorted(cur.get(fam, {}).items(), key=lambda x: -x[1])[:10]
    print(f'  {fam[2:]} ({len(cur.get(fam, {}))} file): ' + ', '.join(f'{f} {n}' for f, n in peggio))
sys.exit(1 if bad else 0)
