#!/usr/bin/env python3
# VesevOS - controllo degli strati: l'hardware si tocca SOLO nei driver (vedi docs/RELEASE_DEVELOPER.md).
# Per ogni famiglia di chiamate hardware conta le righe di codice (commenti e testi tra virgolette esclusi) fuori dal driver.
# Seconda parte: la DIPENDENZA DA ARDUINO (String, Serial, millis/micros/delay, Arduino.h). Dalla fase 5 (Arduino tolto come componente)
# non e ammessa da nessuna parte (solo third_party/); il debito in baseline deve essere vuoto.
# 21/11/2026 (decisione dell'utente): REGOLA DI ADIACENZA. Un livello include solo quello subito sotto, mai uno piu lontano e mai verso l'alto (verso l'alto: ganci). La famiglia
# "salti" conta gli include che saltano uno o piu strati, per coppia da->a; il debito di partenza sta in baseline e puo solo scendere. "ordine" = include verso l'alto: sempre errore.
# La tabella ALLOWED e ora quella stretta (solo il livello subito sotto). Prova del controllo stesso: python3 tools/checklayers.py --prova
# Il "debito" di oggi sta in tools/layers_baseline.json: puo solo calare. Un file nuovo o un conteggio piu alto = errore.
# Uso:  python3 tools/checklayers.py            (controlla)
#       python3 tools/checklayers.py --update   (riscrive il debito dopo aver tolto accessi; SI RIFIUTA se qualcosa e salito o e nuovo: uscita 1, niente scritto)
#       python3 tools/checklayers.py --prova    (prova il controllo su file finti: deve dire OK)
import json, os, re, sys
HERE = os.path.dirname(os.path.abspath(__file__)); SRC = os.path.join(HERE, '..', 'VesevOS')
BASE = os.path.join(HERE, 'layers_baseline.json')
# famiglia -> (espressione, file driver che possono usarla; vos_license_data.h e solo testo di licenze)
RULES = {
  'ble':    (r'\bNimBLE\w*|\bBLEDevice\b',                    {'vos_drv_ble.cpp', 'vos_drv_ble.h', 'vos_license_data.h'}),
  'elf':    (r'\besp_elf_\w+|\belf_set_symbol_resolver\b|\besp_elf\.h', {'vos_drv_elf.cpp', 'vos_drv_elf.h'}),   # caricatore dei moduli ELF: solo il suo driver
  'espnow': (r'\besp_now_\w+',                                 {'vos_drv_espnow.cpp', 'vos_drv_espnow.h'}),
  'mqtt':   (r'\besp_mqtt_client_\w+|\bmqtt_client\.h',        {'vos_drv_mqtt.cpp', 'vos_drv_mqtt.h'}),
  'https':  (r'\besp_http_client_\w+|\besp_http_client\.h',    {'vos_drv_https.cpp', 'vos_drv_https.h'}),
  'wifi':   (r'\bWiFi\.|\besp_wifi_\w+',                       {'vos_drv_espnow.cpp', 'vos_drv_wifi.cpp', 'vos_drv_wifi.h', 'vos_drv_wps.cpp'}),     # il driver ESP-NOW legge solo modo e canale
  'fs':     (r'\bLittleFS\b(?!\.h)|\besp_(?:vfs_)?littlefs\w*', {'vos_drv_fs.cpp', 'vos_drv_fs.h', 'vos_license_data.h'}),
  'ota':    (r'\besp_partition_\w+|\besp_ota_\w+',                {'vos_drv_ota.cpp', 'vos_drv_ota.h'}),
  'i2c':    (r'\bWire\.|\bTwoWire\b|\bWire\.h',               {'vos_drv_i2c.cpp', 'vos_drv_i2c.h'}),
  'gpio':   (r'\b(digitalWrite|digitalRead|pinMode|analogRead|ledcWrite|ledcAttach|neopixelWrite)\s*\(', {'vos_drv_gpio.cpp'}),
  'nvs':    (r'\bPreferences\b|\bnvs_(?:open|get|set|commit|erase)\w*',  {'vos_drv_nvs.h', 'vos_drv_nvs.cpp'}),          # NVS: solo DrvNvs
  'mdns':   (r'\bMDNS\.|\bmdns_\w+',                          {'vos_drv_mdns.cpp', 'vos_drv_mdns.h'}),
  'udp':    (r'\bAsyncUDP\b|\blwip_(?:socket|bind|recvfrom|sendto)\b', {'vos_drv_udp.cpp', 'vos_drv_udp.h', 'vos_drv_tcpsrv.cpp', 'vos_drv_tcpsrv.h'}),         # socket UDP: solo DrvUdp
  'tcp':    (r'\blwip_(?:listen|accept|recv|send|shutdown)\b',  {'vos_drv_tcpsrv.cpp', 'vos_drv_tcpsrv.h'}),          # socket TCP: solo DrvTcpMini
  'rmt':    (r'\brmt_\w+|\brgbLedWrite\b',                    {'vos_drv_led.cpp', 'vos_drv_led.h'}),
  'adc':    (r'\badc_(?:oneshot|cali)_\w+',                    {'vos_drv_gpio.cpp'}),
  'restart': (r'\bESP\.restart\b|\besp_restart\s*\(',              {'vos_drv_sys.cpp'}),                             # riavvio: solo sysReboot -> driver
}
# STRATI: core, devices, net, security, drivers e boards NON includono packages/ o interfaces/ (usano core/vos_hooks.h e il
# registro dei servizi). Nessun debito ammesso: deve restare 0. Solo src/vos_main.cpp e src/vos_wire.cpp collegano tutto.
LOW = ('core', 'devices', 'net', 'security', 'drivers', 'boards')
INC_UP = re.compile(r'^\s*#\s*include\s*"(?:\.\./)*(?:packages|interfaces)/')
# ORDINE dei livelli (docs/progetto/REGOLE-ARCHITETTURA.md): 0 boards, 1 drivers, 2 core, 3 security, 4 devices, 5 net, 6 packages, 7 interfaces.
# TABELLA delle coppie CONSENTITE "cartella -> cartelle che puo includere" (derivata dalle dipendenze vere di oggi, senza le violazioni).
# Ogni altra coppia (verso l'alto, ma anche verso il basso se non e in tabella) e debito: coppia "da->a" in baseline, puo solo scendere;
# una coppia nuova = errore. Per aprire una coppia nuova si cambia la tabella QUI e in REGOLE-ARCHITETTURA.md, di proposito.
# Fuori scala: third_party/ (lo usano tutti), src/vos_wire.* e src/vos_main.cpp (livello 8, collegano tutto).
LEVEL = {'boards': 0, 'drivers': 1, 'core': 2, 'security': 3, 'devices': 4, 'net': 5, 'packages': 6, 'interfaces': 7}
ALLOWED = {c: ({n for n in LEVEL if LEVEL[n] == LEVEL[c] - 1}) for c in LEVEL}   # REGOLA DI ADIACENZA: ogni livello puo includere SOLO quello subito sotto
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
SCHEDA_GEN = {'vos_license_data.h', 'vos_lang_data.h'}
def SCHEDA_OK(f, path):
    return (os.sep + 'boards' + os.sep in path or os.sep + 'third_party' + os.sep in path or f in SCHEDA_GEN or (f.startswith('vos_page') and f.endswith('.h')))
def ARDUINO_OK(f, path):
    return os.sep + 'third_party' + os.sep in path      # fase 5: Arduino non c'e piu, nemmeno nei driver (vos_drv_*): tutto deve restare a 0
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
            if m and me in LEVEL and m.group(1) in LEVEL and LEVEL[m.group(1)] > LEVEL[me]:   # verso l'alto: mai (si usano i ganci); i salti verso il basso stanno in 'salti'
                out.setdefault('ordine', {}); k = me + '->' + m.group(1); out['ordine'][k] = out['ordine'].get(k, 0) + 1
            if m and me in LEVEL and m.group(1) in LEVEL and LEVEL[me] - LEVEL[m.group(1)] > 1:      # SALTO: un livello include uno strato che non e quello subito sotto
                out.setdefault('salti', {}); k = me + '->' + m.group(1); out['salti'][k] = out['salti'].get(k, 0) + 1
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
                if f in ok or os.sep + 'third_party' + os.sep in path: continue      # third_party e fuori scala (copie di librerie, es. il caricatore ELF)
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
def rapporto(cur, base):
    """Confronta il conteggio di oggi con il debito di partenza. Ritorna (errori, righe di riepilogo)."""
    err, righe = [], []
    for fam in RULES:
        for f, n in cur.get(fam, {}).items():
            b = base.get(fam, {}).get(f, 0)
            if n > b: err.append(f"KO {fam}: {f} tocca l'hardware {n} volte (ammesso {b}). Usa il driver.")
    for f, n in cur.get('strati', {}).items():
        err.append(f'KO strati: {f} include packages/ o interfaces/ {n} volte. Lo strato basso usa core/vos_hooks.h o il registro dei servizi; il collegamento sta in src/vos_wire.cpp.')
    for k, n in sorted(cur.get('ordine', {}).items()):
        b = base.get('ordine', {}).get(k, 0)
        if n > b: err.append(f'KO ordine: {k} include verso l alto {n} volte (ammesso {b}). Verso l alto usa core/vos_hooks.h o il registro dei servizi (il collegamento sta in src/vos_wire.cpp).')
    for k, n in sorted(cur.get('salti', {}).items()):
        b = base.get('salti', {}).get(k, 0)
        if n > b: err.append(f"KO salti: {k} salta uno o piu strati {n} volte (ammesso {b}). Regola dell'utente (21/11/2026): un livello parla solo con quello adiacente, passando da ganci, API e interfacce.")
    for f, n in cur.get('e_idf', {}).items():
        b = base.get('e_idf', {}).get(f, 0)
        if n > b: err.append(f'KO e_idf: {f} include {n} header di ESP-IDF (esp_*.h, driver/*.h) fuori dai driver (ammesso {b}). Metti la chiamata in un driver (vos_drv_*).')
    for f, n in cur.get('f_rtos', {}).items():
        b = base.get('f_rtos', {}).get(f, 0)
        if n > b: err.append(f'KO f_rtos: {f} usa primitive FreeRTOS {n} volte fuori da drivers/ e core/ (ammesso {b}). Task, code e semafori si creano nel driver o nel nucleo.')
    for fam, nome in (('cicli', 'moduli'), ('cicli_cart', 'cartelle')):
        bc = [set(k.split(',')) for k in base.get(fam, {})]
        for k in cur.get(fam, {}):
            if not any(set(k.split(',')) <= b for b in bc): err.append(f'KO {fam}: ciclo tra {nome} nuovo o piu grande: {k.replace(",", " <-> ")}. Si rompe con un gancio (core/vos_hooks.h) o spostando il pezzo comune sotto.')
    for fam in ARDUINO:
        for f, n in cur.get(fam, {}).items():
            b = base.get(fam, {}).get(f, 0)
            if n > b: err.append(f'KO {fam}: {f} usa Arduino ({HINT[fam]}) {n} volte (ammesso {b}). Non aggiungere dipendenze da Arduino fuori dai driver: metti il codice nuovo in un driver (vos_drv_*) o senza Arduino.')
    for fam in SCHEDA:
        for f, n in cur.get(fam, {}).items():
            b = base.get(fam, {}).get(f, 0)
            if n > b: err.append(f'KO {fam}: {f} ha {n} valori della scheda scritti a mano ({SHINT[fam]}; ammessi {b}). Mettili nel file di scheda (VesevOS/src/boards/).')
    tot = {fam: sum(cur.get(fam, {}).values()) for fam in list(RULES) + list(ARDUINO) + list(SCHEDA)}
    old = {fam: sum(base.get(fam, {}).values()) for fam in list(RULES) + list(ARDUINO) + list(SCHEDA)}
    righe.append('accessi hardware fuori dai driver (ora / debito di partenza): ' + ', '.join(f'{k} {tot[k]}/{old[k]}' for k in RULES))
    righe.append('include verso strati alti da core/devices/net/security/drivers/boards: ' + str(sum(cur.get('strati', {}).values())) + ' (deve essere 0)')
    righe.append('include verso l alto, coppie (ora / debito di partenza): ' + str(sum(cur.get('ordine', {}).values())) + ' / ' + str(sum(base.get('ordine', {}).values())) + ' - ' + ', '.join(f'{k} {n}/{base.get("ordine", {}).get(k, 0)}' for k, n in sorted(cur.get('ordine', {}).items())))
    righe.append('salti di strato, regola "un livello alla volta" (ora / debito di partenza): ' + str(sum(cur.get('salti', {}).values())) + ' / ' + str(sum(base.get('salti', {}).values())) + ' - ' + ', '.join(f'{k} {n}/{base.get("salti", {}).get(k, 0)}' for k, n in sorted(cur.get('salti', {}).items(), key=lambda x: -x[1])))
    righe.append('include di ESP-IDF fuori dai driver (ora / debito di partenza): ' + str(sum(cur.get('e_idf', {}).values())) + ' / ' + str(sum(base.get('e_idf', {}).values())) + ' in ' + str(len(cur.get('e_idf', {}))) + ' file; primitive FreeRTOS fuori da drivers/ e core/: ' + str(sum(cur.get('f_rtos', {}).values())) + ' / ' + str(sum(base.get('f_rtos', {}).values())) + ' in ' + str(len(cur.get('f_rtos', {}))) + ' file')
    righe.append('cicli tra moduli (gruppi, ora / debito di partenza): ' + str(len(cur.get('cicli', {}))) + ' / ' + str(len(base.get('cicli', {}))) + '  cicli tra cartelle: ' + str(len(cur.get('cicli_cart', {}))) + ' (deve essere 0)')
    for k in cur.get('cicli', {}): righe.append('  ciclo: ' + k.replace(',', ' <-> '))
    righe.append('dipendenza da Arduino fuori dai driver (ora / debito di partenza): ' + ', '.join(f'{k[2:]} {tot[k]}/{old[k]}' for k in ARDUINO))
    righe.append('valori della scheda scritti a mano fuori da boards/ (ora / debito di partenza): ' + ', '.join(f'{k[2:]} {tot[k]}/{old[k]}' for k in SCHEDA))
    for fam in list(SCHEDA) + list(ARDUINO):
        peggio = sorted(cur.get(fam, {}).items(), key=lambda x: -x[1])[:10]
        righe.append(f'  {fam[2:]} ({len(cur.get(fam, {}))} file): ' + ', '.join(f'{f} {n}' for f, n in peggio))
    return err, righe

def debito_aperto(cur):
    return sum(sum(v.values()) for k, v in cur.items() if k not in ('cicli', 'cicli_cart')) + len(cur.get('cicli', {}))

def saliti(cur, base):
    """Voci SALITE rispetto alla baseline: lista di (famiglia, voce, prima, dopo). Una voce nuova ha prima = 0.
    Vale per ogni famiglia (conteggi per file o per coppia). Per i cicli conta un gruppo nuovo (non contenuto in uno gia in baseline)."""
    out = []
    for fam in sorted(cur):
        if fam in ('cicli', 'cicli_cart'):
            bc = [set(k.split(',')) for k in base.get(fam, {})]
            for k in sorted(cur[fam]):
                if not any(set(k.split(',')) <= b for b in bc): out.append((fam, k.replace(',', ' <-> '), 0, 1))
            continue
        for voce, n in sorted(cur[fam].items()):
            prima = base.get(fam, {}).get(voce, 0)
            if n > prima: out.append((fam, voce, prima, n))
    return out

def totali(dati):
    """Totale per famiglia (per i cicli: numero di gruppi)."""
    return {fam: (len(v) if fam in ('cicli', 'cicli_cart') else sum(v.values())) for fam, v in dati.items()}

def aggiorna(cur):
    """--update: riscrive la baseline SOLO se nessuna voce e salita e nessuna voce e nuova. Altrimenti non scrive niente e ritorna 1.
    Senza baseline esistente la base e vuota: se c'e debito, ogni voce e "nuova" e si rifiuta; con debito zero si scrive.
    Nessuna opzione per forzare (non richiesta dall'utente): per accettare un salto si cambia il codice, non la baseline."""
    base = json.load(open(BASE)) if os.path.exists(BASE) else {}
    su = saliti(cur, base)
    if su:
        print('Voci SALITE rispetto alla baseline (famiglia, voce, prima -> dopo):')
        for fam, voce, prima, dopo in su: print(f'  {fam}  {voce}  {prima} -> {dopo}')
        print(f'RISULTATO checklayers --update: RIFIUTATO ({len(su)} voci salite)')
        return 1
    prima, dopo = totali(base), totali(cur)
    for fam in sorted(set(prima) | set(dopo)):
        if dopo.get(fam, 0) < prima.get(fam, 0): print(f'  sceso {fam}: {prima.get(fam, 0)} -> {dopo.get(fam, 0)}')
    with open(BASE, 'w') as f: json.dump(cur, f, indent=1, sort_keys=True)
    print('RISULTATO checklayers --update: scritto')
    return 0

def prova():
    """Prova il controllo stesso su file finti (cartella temporanea): include giusti, salti, include verso l'alto, commenti, baseline, --update."""
    import tempfile, shutil, io, contextlib
    global SRC, BASE
    vecchio = (SRC, BASE)
    d = tempfile.mkdtemp(prefix='prova_checklayers_')
    esiti = []
    def scrivi(rel, testo):
        p = os.path.join(d, 'src', rel); os.makedirs(os.path.dirname(p), exist_ok=True); open(p, 'w').write(testo)
    def attendi(nome, ok): esiti.append((nome, ok)); print(('ok   ' if ok else 'KO   ') + nome)
    try:
        for c in LEVEL: scrivi(f'{c}/vos_{c}.h', '#pragma once\n')
        scrivi('net/giusto.cpp', '#include "../devices/vos_devices.h"\n')                    # adiacente verso il basso: va bene
        scrivi('packages/salto.cpp', '#include "../core/vos_core.h"\n#include "../net/vos_net.h"\n')   # packages -> core salta 3 strati
        scrivi('core/su.cpp', '#include "../drivers/vos_drivers.h"\n#include "../security/vos_security.h"\n')   # core -> security: verso l'alto
        scrivi('interfaces/commenti.cpp', '// #include "../core/vos_core.h"\n/* #include "../drivers/vos_drivers.h" */\n#include "../packages/vos_packages.h"\n')   # commenti ignorati
        scrivi('devices/idf.cpp', '#include <esp_sleep.h>\n')                                    # ESP-IDF fuori dai driver
        SRC = d; BASE = os.path.join(d, 'baseline.json')
        cur = scan()
        attendi("adiacente verso il basso (net -> devices) non e un salto", 'net->devices' not in cur.get('salti', {}) and 'net->devices' not in cur.get('ordine', {}))
        attendi("packages -> core e un salto (e solo quello)", cur.get('salti', {}).get('packages->core') == 1 and len(cur.get('salti', {})) == 1)
        attendi("core -> security e un include verso l'alto", cur.get('ordine', {}).get('core->security') == 1)
        attendi("include dentro i commenti ignorati", 'interfaces->core' not in cur.get('salti', {}) and 'interfaces->drivers' not in cur.get('salti', {}))
        attendi("esp_sleep.h fuori dai driver contato", cur.get('e_idf', {}).get('idf.cpp') == 1)
        err, _ = rapporto(cur, {})
        attendi("senza baseline: salto, verso l'alto ed ESP-IDF danno KO", any('KO salti' in e for e in err) and any('KO ordine' in e for e in err) and any('KO e_idf' in e for e in err))
        err, _ = rapporto(cur, cur)
        attendi("con la baseline uguale al conteggio: nessun errore", not err)
        peggio = json.loads(json.dumps(cur)); peggio['salti']['packages->core'] = 2
        err, _ = rapporto(peggio, cur)
        attendi("un salto in piu rispetto alla baseline: KO", any('KO salti' in e for e in err))
        meglio = json.loads(json.dumps(cur)); del meglio['salti']
        err, _ = rapporto(meglio, cur)
        attendi("un salto in meno rispetto alla baseline: va bene", not any('KO salti' in e for e in err))
        # --- prove di --update (file di baseline temporaneo: BASE punta alla cartella finta, mai a tools/layers_baseline.json) ---
        def leggi(): return open(BASE, 'rb').read() if os.path.exists(BASE) else None
        def esegui():
            buf = io.StringIO()
            with contextlib.redirect_stdout(buf): codice = aggiorna(scan())
            return codice, buf.getvalue().strip().splitlines()[-1]
        open(BASE, 'w').write(json.dumps(cur, indent=1, sort_keys=True))
        prima = leggi(); codice, ultima = esegui()
        attendi("update (a): baseline uguale al conteggio -> scrive, uscita 0", codice == 0 and ultima.endswith(': scritto') and leggi() == prima)
        scrivi('packages/salto.cpp', '#include "../core/vos_core.h"\n#include "../core/vos_core.h"\n#include "../net/vos_net.h"\n')   # packages -> core: 2 invece di 1
        prima = leggi(); codice, ultima = esegui()
        attendi("update (b): un salto in piu -> RIFIUTATO (1 voci), uscita 1, baseline identica", codice == 1 and ultima.endswith('RIFIUTATO (1 voci salite)') and leggi() == prima)
        scrivi('packages/salto.cpp', '#include "../net/vos_net.h"\n')                       # packages -> core sparisce
        prima = leggi(); codice, ultima = esegui()
        attendi("update (c): un salto in meno -> scrive, uscita 0, baseline cambiata", codice == 0 and ultima.endswith(': scritto') and leggi() != prima and 'packages->core' not in json.loads(leggi()).get('salti', {}))
        scrivi('devices/idf2.cpp', '#include <esp_log.h>\n')                                # voce nuova: file nuovo con include IDF
        prima = leggi(); codice, ultima = esegui()
        attendi("update (d): voce nuova (file nuovo con include IDF) -> RIFIUTATO, baseline identica", codice == 1 and 'RIFIUTATO' in ultima and leggi() == prima)
        os.remove(BASE); codice, ultima = esegui()
        attendi("update (e1): senza baseline e con debito -> RIFIUTATO, file non creato", codice == 1 and 'RIFIUTATO' in ultima and leggi() is None)
        d2 = tempfile.mkdtemp(prefix='prova_checklayers_pulito_')
        try:
            os.makedirs(os.path.join(d2, 'src', 'net')); open(os.path.join(d2, 'src', 'net', 'pulito.cpp'), 'w').write('int x;\n')
            SRC = d2; codice, ultima = esegui()
            attendi("update (e2): senza baseline e senza debito -> scrive, file creato", codice == 0 and ultima.endswith(': scritto') and leggi() is not None)
        finally:
            shutil.rmtree(d2, ignore_errors=True)
    finally:
        SRC, BASE = vecchio
        shutil.rmtree(d, ignore_errors=True)
    print('RISULTATO prova checklayers:', 'OK' if all(o for _, o in esiti) else 'KO', f'({sum(1 for _, o in esiti if o)}/{len(esiti)})')
    return 0 if all(o for _, o in esiti) else 1

def principale():
    cur = scan()
    if '--update' in sys.argv:
        return aggiorna(cur)
    base = json.load(open(BASE)) if os.path.exists(BASE) else {}
    err, righe = rapporto(cur, base)
    for e in err: print(e)
    for r in righe: print(r)
    # Ultima riga SEMPRE: l'esito. Prima il controllo si leggeva solo dalle ultime righe e un KO in testa passava inosservato (21/11/2026).
    print(f'RISULTATO checklayers: {"KO, " + str(len(err)) + " errori" if err else "OK"}; debito tollerato dalla baseline ancora aperto: {debito_aperto(cur)} voci (non e "pulito": e il limite che non deve crescere)')
    return 1 if err else 0

if __name__ == '__main__':
    sys.exit(prova() if '--prova' in sys.argv else principale())
