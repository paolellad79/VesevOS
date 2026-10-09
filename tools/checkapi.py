#!/usr/bin/env python3
# VesevOS - controllo delle rotte API (regole API 1 e B): ogni rotta ha metodo e livello; l'elenco deve coincidere con tools/api_routes.txt.
# Una rotta nuova o un livello cambiato = errore finche non si aggiorna l'elenco (di proposito: obbliga a decidere il livello e a documentarlo).
# Uso:  python3 tools/checkapi.py            (controlla)
#       python3 tools/checkapi.py --update   (riscrive l'elenco dopo una modifica voluta)
import os, re, sys
HERE = os.path.dirname(os.path.abspath(__file__)); SRC = os.path.join(HERE, '..', 'VesevOS')
LIST = os.path.join(HERE, 'api_routes.txt')
LV = {'L_PUB': 'pubblico', 'L_GUEST': 'ospite', 'L_OPER': 'operatore', 'L_ADMIN': 'admin'}
rx = re.compile(r'add\(\s*\w+\s*,\s*\w+\s*,\s*"(/[^"]*)"\s*,\s*HTTP_(\w+)\s*,\s*(L_\w+)')
routes = set()
for root, _, fs in os.walk(SRC):
    for f in fs:
        if f.endswith('.cpp'):
            for line in open(os.path.join(root, f), encoding='utf-8', errors='replace'):
                for m in rx.finditer(line):
                    routes.add(f'{m.group(2):5} {m.group(1)}  {LV.get(m.group(3), m.group(3))}')
cur = sorted(routes, key=lambda x: (x.split()[1], x.split()[0]))
if '--update' in sys.argv:
    open(LIST, 'w').write('\n'.join(cur) + '\n'); print('elenco riscritto:', len(cur), 'rotte'); sys.exit(0)
old = [l.rstrip('\n') for l in open(LIST)] if os.path.exists(LIST) else []
bad = 0
for r in cur:
    if r not in old: print('NUOVA o CAMBIATA:', r); bad += 1
for r in old:
    if r not in cur: print('SPARITA:', r); bad += 1
print(f'rotte API: {len(cur)} (elenco: {len(old)})', 'OK' if not bad else 'DIFFERENZE')
# documento API (docs/API.md e API.en.md) generato dal codice: deve essere allineato e ogni rotta deve avere la descrizione
import subprocess
r = subprocess.run([sys.executable, os.path.join(HERE, 'mkapidoc.py'), '--check'], capture_output=True, text=True)
print((r.stdout + r.stderr).strip())
sys.exit(1 if bad or r.returncode else 0)
