#!/usr/bin/env python3
# VesevOS - tools/mkzip-github.py
# Prepara UN SOLO zip per GitHub: sorgenti del repo (filtrati) + sito (cartella sito-github/ accanto al repo).
# Uso (dalla cartella del repo): python3 tools/mkzip-github.py <versione>
# Scrive ../consegna/VesevOS-github-<versione>.zip, stampa dimensione, SHA-256, controlli e file sospetti (solo nomi).
# Lista completa dei passi: docs/progetto/lista-prima-di-pubblicare.md
import hashlib, os, re, shutil, sys, tempfile, zipfile

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BASE = os.path.dirname(REPO)
SITO = os.path.join(BASE, 'sito-github')
OUT_DIR = os.path.join(BASE, 'consegna')

# Dal repo NON si copia (file interni, prove, dati di lavoro).
ESCLUDI_RADICE = {'CLAUDE.md', 'STATO-tmp.md', 'semaforo-tmp.md', 'COMMIT-1.7.45.txt', 'firmware', 'flash', 'README.md', 'README.en.md'}
ESCLUDI_PERCORSI = ('.claude', 'docs/progetto', 'docs/terze-parti', 'tools/webtest', 'tools/__pycache__', 'VesevOS/build', 'node_modules', '.git')
ESCLUDI_NOMI = {'.DS_Store', '.vesevos-schede', '__pycache__'}
ESCLUDI_EXT = ('.elf', '.pyc', '.log')
# Sospetti: nomi di file con assegnazioni letterali o chiavi (si stampano solo i nomi)
SOSPETTO = re.compile(rb'(BEGIN [A-Z ]*PRIVATE KEY|ghp_[A-Za-z0-9]{20,}|AKIA[0-9A-Z]{16}|(?i:passw(or)?d|token|secret)\w*\s*[=:]\s*"[^"%${}\\\s]{6,}")')


def escluso(rel, nome):
    if nome in ESCLUDI_NOMI or nome.endswith(ESCLUDI_EXT) or nome.startswith('backup-'):
        return True
    return any(rel == p or rel.startswith(p + '/') for p in ESCLUDI_PERCORSI)


def copia(src, dst):
    for d, dirs, files in os.walk(src):
        rel_d = os.path.relpath(d, src)
        rel_d = '' if rel_d == '.' else rel_d
        dirs[:] = [x for x in dirs if not escluso((rel_d + '/' + x).lstrip('/'), x)]
        for f in files:
            rel = (rel_d + '/' + f).lstrip('/')
            if escluso(rel, f):
                continue
            os.makedirs(os.path.join(dst, rel_d), exist_ok=True)
            shutil.copy2(os.path.join(d, f), os.path.join(dst, rel_d, f))


def main():
    if len(sys.argv) != 2:
        sys.exit('uso: mkzip-github.py <versione>')
    ver = sys.argv[1]
    stage = tempfile.mkdtemp(prefix='vos-zip-')
    try:
        # 1) repo senza le cartelle che arrivano dal sito, 2) sito sopra
        for top in os.listdir(REPO):
            p = os.path.join(REPO, top)
            if top in ESCLUDI_RADICE or escluso(top, top):
                continue
            if os.path.isdir(p):
                copia(p, os.path.join(stage, top))
            else:
                shutil.copy2(p, os.path.join(stage, top))
        # le sotto-cartelle di escluso dentro il repo si controllano con il percorso completo
        for rel in ESCLUDI_PERCORSI:
            shutil.rmtree(os.path.join(stage, rel), ignore_errors=True)
        copia(SITO, stage)
        # controlli
        problemi, sospetti, n = [], [], 0
        for d, _, files in os.walk(stage):
            for f in files:
                n += 1
                p = os.path.join(d, f)
                rel = os.path.relpath(p, stage)
                if f.endswith('.elf') or f == '.DS_Store' or f.startswith('backup-'):
                    problemi.append(rel)
                if os.path.getsize(p) < 2_000_000 and f.rsplit('.', 1)[-1] in ('c', 'h', 'cpp', 'py', 'js', 'json', 'md', 'txt', 'html', 'sh', 'cfg', 'be', 'vdb'):
                    if SOSPETTO.search(open(p, 'rb').read()):
                        sospetti.append(rel)
        os.makedirs(OUT_DIR, exist_ok=True)
        out = os.path.join(OUT_DIR, 'VesevOS-github-%s.zip' % ver)
        if os.path.exists(out):
            os.remove(out)
        with zipfile.ZipFile(out, 'w', zipfile.ZIP_DEFLATED) as z:
            for d, _, files in sorted(os.walk(stage)):
                for f in sorted(files):
                    p = os.path.join(d, f)
                    z.write(p, os.path.relpath(p, stage))
        h = hashlib.sha256(open(out, 'rb').read()).hexdigest()
        print('zip:', out)
        print('file:', n, ' byte:', os.path.getsize(out))
        print('sha256:', h)
        print('elf/DS_Store/backup nello zip:', problemi if problemi else 'nessuno')
        print('file sospetti per segreti (solo nomi):', sospetti if sospetti else 'nessuno')
    finally:
        shutil.rmtree(stage, ignore_errors=True)


main()
