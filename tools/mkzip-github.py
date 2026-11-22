#!/usr/bin/env python3
# VesevOS - tools/mkzip-github.py
# Prepara UN SOLO zip per GitHub: sorgenti del repo (filtrati) + sito (cartella sito-github/ accanto al repo).
# Uso (dalla cartella del repo): python3 tools/mkzip-github.py <versione>
# Scrive ../consegna/github/VesevOS-github-<versione>.zip, stampa dimensione, SHA-256, controlli e file sospetti (solo nomi).
# Lista completa dei passi: docs/progetto/lista-prima-di-pubblicare.md
import hashlib, json, os, re, shutil, sys, tempfile, zipfile

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BASE = os.path.dirname(REPO)
SITO = os.path.join(BASE, 'sito-github')
OUT_DIR = os.path.join(BASE, 'consegna', 'github')   # struttura dal 21/11/2026
RISORSE = os.path.join(BASE, 'build', 'risorse', 'usr-share-vesevos')   # file di /usr/share/vesevos scritti da tools/mkrisorse.py
IMMAGINE = os.path.join(BASE, 'build', 'risorse', 'immagine')           # immagine LittleFS corta scritta da tools/mkimmaginerisorse.py

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


# Progetto ESP-IDF (fuori dal repo: ../idf): senza questo chi scarica da GitHub non puo compilare. Si copiano solo i file sorgente,
# mai le cartelle di build, i componenti scaricati (managed_components*), i log e le configurazioni di prova (arduino, qemu).
IDF = os.path.join(BASE, 'idf')
IDF_FILE = ('CMakeLists.txt', 'partitions.csv', 'sdkconfig.defaults.vesevos', 'sdkconfig.defaults.c3', 'sdkconfig.defaults.c6',
            'dependencies.lock', 'dependencies.lock.c3', 'dependencies.lock.c6', 'lwip_dhcp_fix.h')
IDF_CARTELLE = ('main', 'components')

def copia_idf(stage):
    dst = os.path.join(stage, 'idf')
    os.makedirs(dst, exist_ok=True)
    for nome in IDF_FILE:
        src = os.path.join(IDF, nome)
        if not os.path.isfile(src):
            sys.exit('manca idf/%s: il progetto ESP-IDF non e completo' % nome)
        shutil.copy2(src, os.path.join(dst, nome))
    for cart in IDF_CARTELLE:
        copia(os.path.join(IDF, cart), os.path.join(dst, cart))

def versione_firmware():
    # VOS_VERSION in core/vos_common.h: la cartella risorse/<versione>/ deve avere quella del firmware, perche e la pagina a costruire l'indirizzo
    t = open(os.path.join(REPO, 'VesevOS', 'src', 'core', 'vos_common.h'), encoding='ascii').read()
    m = re.search(r'#define\s+VOS_VERSION\s+"([^"]+)"', t)
    if not m:
        sys.exit('VOS_VERSION non trovata in core/vos_common.h')
    return m.group(1)


def copia_risorse(stage):
    # risorse/<VOS_VERSION>/<file>: gli stessi file di /usr/share/vesevos, da cui il browser li scarica quando la scheda ne ha uno diverso o mancante.
    # Si controllano prima con SHA256SUMS (formato di sha256sum): un file che non coincide ferma lo zip.
    if not os.path.isdir(RISORSE):
        sys.exit('manca %s: lanciare prima tools/mkrisorse.py (senza --out)' % RISORSE)
    voci = {}
    for riga in open(os.path.join(RISORSE, 'SHA256SUMS'), encoding='ascii').read().splitlines():
        h, nome = riga.split('  ', 1)
        voci[nome] = h
    for nome in os.listdir(RISORSE):
        if nome != 'SHA256SUMS' and hashlib.sha256(open(os.path.join(RISORSE, nome), 'rb').read()).hexdigest() != voci.get(nome):
            sys.exit('risorse: %s non coincide con SHA256SUMS: rilanciare tools/mkrisorse.py' % nome)
    mancanti = [n for n in voci if not os.path.isfile(os.path.join(RISORSE, n))]
    if mancanti:
        sys.exit('risorse: file dell\'elenco mancanti: %s' % mancanti)
    dst = os.path.join(stage, 'risorse', versione_firmware())
    os.makedirs(dst, exist_ok=True)
    for nome in sorted(os.listdir(RISORSE)):
        shutil.copy2(os.path.join(RISORSE, nome), os.path.join(dst, nome))
    return versione_firmware(), len(os.listdir(dst))


def copia_immagine(stage):
    # firmware/risorse/<VOS_VERSION>/VesevOS-Risorse-<VOS_VERSION>.bin (+ .sha256): l'immagine che l'installatore (flash/) scrive SOLO
    # nell'installazione completa. Si controlla lo SHA-256 e che lo stesso valore sia in flash/firmware.json: se no lo zip si ferma.
    ver = versione_firmware()
    nome = 'VesevOS-Risorse-%s.bin' % ver
    sorgente = os.path.join(IMMAGINE, nome)
    if not os.path.isfile(sorgente) or not os.path.isfile(sorgente + '.sha256'):
        sys.exit('manca %s: lanciare tools/mkimmaginerisorse.py' % sorgente)
    dati = open(sorgente, 'rb').read()
    sha = hashlib.sha256(dati).hexdigest()
    if open(sorgente + '.sha256', encoding='ascii').read().split()[0] != sha:
        sys.exit('immagine risorse: %s.sha256 non coincide con il file: rilanciare tools/mkimmaginerisorse.py' % nome)
    elenco = json.load(open(os.path.join(stage, 'flash', 'firmware.json'), encoding='utf-8'))
    parti = [p for p in elenco['parti'].values() if p.get('tipo') == 'risorse']
    if len(parti) != 1 or parti[0]['sha256'] != sha or parti[0]['dimensione'] != len(dati) or parti[0]['file'] != '../firmware/risorse/%s/%s' % (ver, nome):
        sys.exit('flash/firmware.json non descrive questa immagine (percorso, dimensione, SHA-256): aggiornare la parte di tipo risorse')
    dst = os.path.join(stage, 'firmware', 'risorse', ver)
    os.makedirs(dst, exist_ok=True)
    shutil.copy2(sorgente, os.path.join(dst, nome))
    shutil.copy2(sorgente + '.sha256', os.path.join(dst, nome + '.sha256'))
    return nome, len(dati), sha


def main():
    args = [a for a in sys.argv[1:] if a != '--a-secco']
    a_secco = len(args) != len(sys.argv) - 1          # --a-secco: fa tutti i controlli ma non scrive lo zip
    if len(args) != 1:
        sys.exit('uso: mkzip-github.py <versione> [--a-secco]')
    ver = args[0]
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
        copia_idf(stage)
        ver_ris, n_ris = copia_risorse(stage)
        nome_img, byte_img, sha_img = copia_immagine(stage)
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
        if a_secco:
            print('A SECCO: lo zip NON e stato creato. File che ci sarebbero in firmware/risorse e risorse:')
            for sotto in ('firmware/risorse', 'risorse'):
                for d, _, files in sorted(os.walk(os.path.join(stage, sotto))):
                    for f in sorted(files):
                        print('  ', os.path.relpath(os.path.join(d, f), stage), os.path.getsize(os.path.join(d, f)))
            print('file:', n, ' elf/DS_Store/backup:', problemi if problemi else 'nessuno', ' sospetti:', sospetti if sospetti else 'nessuno')
            print('immagine risorse: %s, %d byte, sha256 %s, descritta da flash/firmware.json' % (nome_img, byte_img, sha_img))
            return
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
        print('risorse/%s/: %d file (stessi di /usr/share/vesevos)%s' % (ver_ris, n_ris, '' if ver_ris == ver else '  ATTENZIONE: la versione dello zip (%s) e diversa da VOS_VERSION' % ver))
        print('firmware/risorse/%s/%s: %d byte, sha256 %s (immagine per l\'installazione completa)' % (ver_ris, nome_img, byte_img, sha_img))
        print('file:', n, ' byte:', os.path.getsize(out))
        print('sha256:', h)
        print('elf/DS_Store/backup nello zip:', problemi if problemi else 'nessuno')
        print('file sospetti per segreti (solo nomi):', sospetti if sospetti else 'nessuno')
    finally:
        shutil.rmtree(stage, ignore_errors=True)


main()
