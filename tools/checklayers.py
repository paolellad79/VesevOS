#!/usr/bin/env python3
# VesevOS - controllo degli strati: l'hardware si tocca SOLO nei driver (claude/VesevOS-schema-strati.md).
# Per ogni famiglia di chiamate hardware conta le righe di codice (commenti e testi tra virgolette esclusi) fuori dal driver.
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
  'wifi':   (r'\bWiFi\.|\besp_wifi_\w+',                       {'vos_drv_espnow.cpp', 'vos_drv_wifi.cpp', 'vos_drv_wifi.h'}),     # il driver ESP-NOW legge solo modo e canale
  'fs':     (r'\bLittleFS\b(?!\.h)',                           {'vos_drv_fs.cpp', 'vos_drv_fs.h', 'vos_license_data.h'}),
  'ota':    (r'\besp_partition_\w+|\besp_ota_\w+',                {'vos_drv_ota.cpp', 'vos_drv_ota.h'}),
  'gpio':   (r'\b(digitalWrite|digitalRead|pinMode|analogRead|ledcWrite|ledcAttach|neopixelWrite)\s*\(', {'vos_drv_gpio.cpp'}),
}
def code_only(line):
    line = re.sub(r'"(\\.|[^"\\])*"', '""', line)          # testi
    line = re.sub(r'//.*', '', line)                        # commento di riga
    return line
def scan():
    out = {}
    allf = []
    for root, _, fs in os.walk(SRC):
        for f in fs:
            if f.endswith(('.cpp', '.h', '.ino')): allf.append((f, os.path.join(root, f)))
    for f, path in sorted(allf):
        inblock = False
        for line in open(path, encoding='utf-8', errors='replace'):
            if inblock:
                if '*/' in line: inblock = False; line = line.split('*/', 1)[1]
                else: continue
            if '/*' in line and '*/' not in line: inblock = True; line = line.split('/*', 1)[0]
            c = code_only(line)
            for fam, (rx, ok) in RULES.items():
                if f in ok: continue
                n = len(re.findall(rx, c))
                if n: out.setdefault(fam, {}); out[fam][f] = out[fam].get(f, 0) + n
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
tot = {fam: sum(cur.get(fam, {}).values()) for fam in RULES}
old = {fam: sum(base.get(fam, {}).values()) for fam in RULES}
print('accessi hardware fuori dai driver (ora / debito di partenza):', ', '.join(f'{k} {tot[k]}/{old[k]}' for k in RULES))
sys.exit(1 if bad else 0)
