#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# SPDX-License-Identifier: GPL-3.0-or-later
# Genera il file unico dei dati comuni "common" (paesi + mappa + regole radio + elenco librerie)
# e VesevOS/vos_common_data.h (blocco compresso servito dalla pagina + tabella radio per il firmware).
#
# Uso normale (non serve internet):        python3 tools/mkcommon.py
# Rigenerare i dati sorgente (una volta):   python3 tools/mkcommon.py --sources <ne_110m.geojson> <ne_50m.geojson> <zone.tab> <iso3166.tab> <zones.json>
#   ne_*  = Natural Earth "admin 0 countries" (pubblico dominio)   https://github.com/nvkelso/natural-earth-vector
#   zone.tab, iso3166.tab = IANA tz database (pubblico dominio)       https://github.com/eggert/tz
#   zones.json = posix_tz_db (licenza MIT)                            https://github.com/nayarsystems/posix_tz_db
# Non e consulenza legale: le regole radio sono semplificate e prudenti; vanno ricontrollate prima della vendita.
import os, sys, json, gzip, math

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, 'tools', 'data', 'common_src.json')
OUT = os.path.join(ROOT, 'VesevOS', 'vos_common_data.h')
BUDGET = 100 * 1024          # massimo compresso

# ---------- regole radio 2,4 GHz (semplificate, prudenti) ----------
# canali 1-11: Americhe del Nord e Taiwan; altrove 1-13 (il 14 del Giappone e escluso: solo 802.11b)
CH11 = {'US', 'CA', 'PR', 'GU', 'VI', 'AS', 'MP', 'UM', 'TW'}
# EIRP massima in dBm: 20 (Europa e la maggior parte del mondo); 30 dove la legge ammette di piu
DBM30 = {'US', 'CA', 'PR', 'GU', 'VI', 'AS', 'MP', 'UM', 'AU', 'NZ', 'BR', 'IN', 'MX', 'AR', 'CL', 'CO'}
REGIONS = [
    {'id': 'eu', 'rule': 'RED 2014/53/UE, ETSI EN 300 328, EN 18031', 'mark': 'CE'},
    {'id': 'gb', 'rule': 'Radio Equipment Regulations 2017, PSTI Act 2022', 'mark': 'UKCA'},
    {'id': 'us', 'rule': 'FCC Part 15.247', 'mark': 'FCC ID'},
    {'id': 'ca', 'rule': 'ISED RSS-247', 'mark': 'IC'},
    {'id': 'jp', 'rule': 'MIC / ARIB STD-T66', 'mark': 'TELEC'},
    {'id': 'cn', 'rule': 'SRRC', 'mark': 'SRRC'},
    {'id': 'in', 'rule': 'WPC (delicensed 2.4 GHz)', 'mark': 'WPC'},
    {'id': 'au', 'rule': 'ACMA LIPD Class Licence', 'mark': 'RCM'},
    {'id': 'br', 'rule': 'Anatel', 'mark': 'Anatel'},
    {'id': 'xx', 'rule': 'ITU-R, banda ISM 2400-2483.5 MHz', 'mark': ''},
]
EU = set('AT BE BG HR CY CZ DK EE FI FR DE GR HU IE IT LV LT LU MT NL PL PT RO SK SI ES SE IS LI NO CH SM VA MC AD'.split())
def region_of(cc):
    if cc in EU: return 'eu'
    if cc == 'GB': return 'gb'
    if cc in ('US', 'PR', 'GU', 'VI', 'AS', 'MP', 'UM'): return 'us'
    if cc == 'CA': return 'ca'
    if cc == 'JP': return 'jp'
    if cc == 'CN': return 'cn'
    if cc == 'IN': return 'in'
    if cc in ('AU', 'NZ'): return 'au'
    if cc == 'BR': return 'br'
    return 'xx'

# ---------- formati locali (semplificati) ----------
FAHRENHEIT = {'US', 'PR', 'GU', 'VI', 'AS', 'MP', 'UM', 'LR', 'MM', 'BS', 'BZ', 'KY', 'PW', 'FM', 'MH'}
MDY = {'US', 'PR', 'GU', 'VI', 'AS', 'MP', 'UM', 'FM', 'MH', 'PW'}
YMD = {'CN', 'JP', 'KR', 'KP', 'TW', 'HU', 'LT', 'MN', 'SE', 'IR', 'BT'}
H12 = {'US', 'PR', 'GU', 'VI', 'AS', 'MP', 'UM', 'CA', 'AU', 'NZ', 'IN', 'PK', 'BD', 'PH', 'EG', 'SA', 'MY', 'CO', 'MX'}
SUNDAY = {'US', 'PR', 'GU', 'VI', 'AS', 'MP', 'UM', 'CA', 'JP', 'BR', 'MX', 'IL', 'PH', 'KR', 'TW', 'HK', 'IN', 'SA', 'ZA', 'AU', 'CN', 'MO', 'PE', 'CO', 'GT', 'HN', 'NI', 'PA', 'SV', 'DO', 'VE', 'ZW', 'KE', 'TH', 'SG', 'MY', 'ID', 'PK', 'BZ', 'BS', 'JM', 'TT'}
DOT = {'US', 'PR', 'GU', 'VI', 'AS', 'MP', 'UM', 'GB', 'IE', 'CA', 'AU', 'NZ', 'IN', 'PK', 'BD', 'JP', 'CN', 'KR', 'TW', 'HK', 'SG', 'MY', 'PH', 'TH', 'MX', 'IL', 'NG', 'KE', 'GH', 'EG', 'SA', 'AE', 'CH', 'LI', 'DO', 'GT', 'HN', 'NI', 'PA', 'SV', 'PE', 'BW', 'ZW', 'MT'}
LANG = {'IT': 'it', 'SM': 'it', 'VA': 'it', 'CH': 'it', 'ES': 'es', 'MX': 'es', 'AR': 'es', 'CO': 'es', 'CL': 'es', 'PE': 'es', 'VE': 'es',
        'EC': 'es', 'GT': 'es', 'CU': 'es', 'BO': 'es', 'DO': 'es', 'HN': 'es', 'PY': 'es', 'SV': 'es', 'NI': 'es', 'CR': 'es', 'PA': 'es',
        'UY': 'es', 'GQ': 'es', 'DE': 'de', 'AT': 'de', 'LI': 'de'}

# ---------- elenco del software usato (SBOM semplice) ----------
SBOM = [
    ['VesevOS', '1.7.11', 'GPL-3.0-or-later OR LicenseRef-VesevOS-Commercial', 'https://github.com/paolellad79/VesevOS'],
    ['Arduino-ESP32 core', '3.3.x', 'LGPL-2.1-or-later', 'https://github.com/espressif/arduino-esp32'],
    ['ESP-IDF (esp_wifi, esp_now, esp-mqtt, esp_http_server, esp_https_server, NVS)', '5.5', 'Apache-2.0', 'https://github.com/espressif/esp-idf'],
    ['Mbed TLS', '3.6', 'Apache-2.0', 'https://github.com/Mbed-TLS/mbedtls'],
    ['FreeRTOS', 'ESP-IDF 5.5', 'MIT', 'https://www.freertos.org'],
    ['lwIP', 'ESP-IDF 5.5', 'BSD-3-Clause', 'https://savannah.nongnu.org/projects/lwip'],
    ['LittleFS', 'core 3.3', 'BSD-3-Clause', 'https://github.com/littlefs-project/littlefs'],
    ['NimBLE (Apache Mynewt)', 'core 3.3', 'Apache-2.0', 'https://github.com/apache/mynewt-nimble'],
    ['PsychicHttp', '3.1.2', 'MIT', 'https://github.com/hoeken/PsychicHttp'],
    ['ArduinoJson', '7.x', 'MIT', 'https://github.com/bblanchon/ArduinoJson'],
    ['Natural Earth (nomi dei paesi)', '5.1', 'Public Domain', 'https://www.naturalearthdata.com'],
    ['IANA tz database (fusi orari)', '2025+', 'Public Domain', 'https://www.iana.org/time-zones'],
    ['posix_tz_db (fusi in formato POSIX)', '2025', 'MIT', 'https://github.com/nayarsystems/posix_tz_db'],
]

# territori senza punto di etichetta: longitudine, latitudine approssimate (dati geografici)
SPOT = {'BQ': (-68.27, 12.18), 'CC': (96.87, -12.17), 'CX': (105.69, -10.45), 'GF': (-53.1, 3.93), 'GI': (-5.35, 36.14),
        'GP': (-61.55, 16.25), 'MQ': (-61.02, 14.64), 'RE': (55.54, -21.12), 'SJ': (16.0, 78.0), 'TK': (-171.85, -9.2),
        'UM': (166.6, 19.3), 'YT': (45.17, -12.83)}

def project(lon, lat):
    return (lon + 180.0) * 1000.0 / 360.0, (90.0 - lat) * 500.0 / 180.0

def ring_path(ring):
    pts = []
    for lon, lat in ring:
        x, y = project(lon, lat)
        p = (int(round(x)), int(round(y)))
        if not pts or pts[-1] != p: pts.append(p)
    if len(pts) > 1 and pts[0] == pts[-1]: pts.pop()
    if len(pts) < 3: return ''
    out = 'M%d %d' % pts[0]
    px, py = pts[0]
    rel = []
    for x, y in pts[1:]:
        rel.append('%d %d' % (x - px, y - py)); px, py = x, y
    return out + 'l' + ' '.join(rel).replace(' -', '-') + 'z'

def from_sources(ne110, ne50, zonetab, isotab, zonesjson):
    g110 = json.load(open(ne110, encoding='utf-8'))
    g50 = json.load(open(ne50, encoding='utf-8'))
    posix = json.load(open(zonesjson, encoding='utf-8'))
    zones = {}
    for line in open(zonetab, encoding='utf-8'):
        if line.startswith('#') or not line.strip(): continue
        f = line.rstrip('\n').split('\t')
        for cc in f[0].split(','):
            zones.setdefault(cc, []).append(f[2])
    isonames = {}
    for line in open(isotab, encoding='utf-8'):
        if line.startswith('#') or not line.strip(): continue
        cc, nm = line.rstrip('\n').split('\t')[:2]
        isonames[cc] = nm
    def cc_of(p):
        c = p.get('ISO_A2_EH') or p.get('ISO_A2')
        return c if c and len(c) == 2 and c.isalpha() else None
    countries = {}
    for f in g50['features']:
        p = f['properties']; cc = cc_of(p)
        if not cc: continue
        countries[cc] = {'cc': cc, 'n': {'it': p.get('NAME_IT') or p['NAME'], 'en': p.get('NAME_EN') or p['NAME'],
                                        'es': p.get('NAME_ES') or p['NAME'], 'de': p.get('NAME_DE') or p['NAME']},
                         'lx': round(project(p['LABEL_X'], p['LABEL_Y'])[0]), 'ly': round(project(p['LABEL_X'], p['LABEL_Y'])[1])}
    for cc, nm in isonames.items():           # paesi senza forma in Natural Earth: nome inglese per tutte le lingue
        if cc not in countries and cc in zones:
            countries[cc] = {'cc': cc, 'n': {'it': nm, 'en': nm, 'es': nm, 'de': nm}}
    for f in g110['features']:
        p = f['properties']; cc = cc_of(p)
        if not cc or cc == 'AQ' or cc not in countries: continue
        geom = f['geometry']
        polys = geom['coordinates'] if geom['type'] == 'MultiPolygon' else [geom['coordinates']]
        d = ''.join(ring_path(ring) for poly in polys for ring in poly[:1])
        if d: countries[cc]['d'] = d
    for cc, c in countries.items():
        z = [[t, posix[t]] for t in zones.get(cc, []) if t in posix]
        if z: c['tz'] = z
    out = {'countries': sorted(countries.values(), key=lambda c: c['cc'])}
    os.makedirs(os.path.dirname(SRC), exist_ok=True)
    json.dump(out, open(SRC, 'w', encoding='utf-8'), ensure_ascii=False, separators=(',', ':'))
    print('scritto', SRC, len(out['countries']), 'paesi')

def build():
    src = json.load(open(SRC, encoding='utf-8'))
    cl = []
    table = []
    for c in src['countries']:
        cc = c['cc']
        e = {k: v for k, v in c.items() if k not in ('d', 'lx', 'ly')}   # dalla 1.7.4 senza mappa: solo elenco dei paesi
        e['ch'] = 11 if cc in CH11 else 13
        e['dbm'] = 30 if cc in DBM30 else 20
        e['rg'] = region_of(cc)
        e['f'] = {'t': 1 if cc in FAHRENHEIT else 0, 'd': 2 if cc in MDY else (1 if cc in YMD else 0), 'h': 1 if cc in H12 else 0,
                  'w': 1 if cc in SUNDAY else 0, 's': 1 if cc in DOT else 0, 'l': LANG.get(cc, 'en')}
        e['ntp'] = cc.lower() + '.pool.ntp.org'
        cl.append(e)
        table.append((cc, e['ch'], e['dbm']))
    common = {'v': 1, 'countries': cl, 'regions': REGIONS,
              'default': {'ch': 11, 'dbm': 20},
              'sbom': [{'name': a, 'ver': b, 'lic': c, 'url': d} for a, b, c, d in SBOM]}
    raw = json.dumps(common, ensure_ascii=False, separators=(',', ':')).encode('utf-8')
    gz = gzip.compress(raw, 9, mtime=0)
    if len(gz) > BUDGET:
        print('ERRORE: common compresso %d byte > budget %d' % (len(gz), BUDGET)); sys.exit(1)
    lines = []
    for i in range(0, len(gz), 24):
        lines.append(','.join('0x%02x' % b for b in gz[i:i + 24]))
    h = ['// VesevOS - vos_common_data.h (GENERATO da tools/mkcommon.py, non modificare a mano)',
         '// Dati: Natural Earth (pubblico dominio), IANA tz (pubblico dominio), posix_tz_db (MIT).',
         '#pragma once', '#include <Arduino.h>', '',
         'struct VosCountryRadio { char cc[3]; uint8_t ch; uint8_t dbm; };',
         'static const VosCountryRadio VOS_COUNTRIES[] = {']
    h.append(',\n'.join('  {"%s", %d, %d}' % t for t in table))
    h.append('};')
    h.append('static const int VOS_COUNTRY_N = %d;' % len(table))
    h.append('static const size_t COMMON_GZ_LEN = %d;' % len(gz))
    h.append('static const uint8_t COMMON_GZ[] PROGMEM = {')
    h.append(',\n'.join(lines))
    h.append('};')
    open(OUT, 'w', encoding='ascii', newline='\n').write('\n'.join(h) + '\n')
    print('ok common: %d paesi, %d byte (compresso %d byte, budget %d)' % (len(cl), len(raw), len(gz), BUDGET))

if __name__ == '__main__':
    if len(sys.argv) > 1 and sys.argv[1] == '--sources':
        from_sources(*sys.argv[2:7])
    build()
