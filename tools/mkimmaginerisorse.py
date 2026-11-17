#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# SPDX-License-Identifier: GPL-3.0-or-later
# Crea l'immagine LittleFS CORTA con i file di /usr/share/vesevos, per la PRIMA installazione dal sito (cartella flash/).
# Piano: docs/progetto/COSE-DA-FARE.md#risorse-file-singoli-2026-11-16 ("PRIMA INSTALLAZIONE", passo 8).
#
# Cosa fa:
#   1. controlla i file di build/risorse/usr-share-vesevos (prodotti da tools/mkrisorse.py): ogni file e nell'elenco SHA256SUMS con
#      lo SHA-256 giusto, non ce ne sono altri e html.vrs e valido (nessun id del firmware dal 16/11/2026).
#      Se qualcosa non torna si ferma con un errore.
#   2. compila tools/lfs_immagine.c con il lfs.c del componente joltwallet/littlefs 1.22.3 (lo stesso del firmware) e crea l'immagine
#      con gli stessi parametri del firmware (blocco 4096, lettura e scrittura 128, cache 512, lookahead 128, block_cycles 512).
#   3. l'immagine ha un numero di blocchi CORTO (quelli usati + un margine): il firmware la estende a tutta la partizione al
#      primo montaggio (grow_on_mount in drivers/vos_drv_fs.cpp, lfs_fs_grow in esp_littlefs.c).
#   4. scrive VesevOS-Risorse-<versione>.bin e il suo .bin.sha256 (stesso formato dei .sha256 del firmware).
# Riproducibile: nessuna data nell'immagine (nessun attributo mtime: la memoria interna non mostra le date), nessun numero a caso.
# Nome del file: indipendente dalla CPU (vedi .claude/rules/nomi-dei-pacchetti.md e la decisione del 16/11/2026).
#
# Uso: python3 tools/mkimmaginerisorse.py [--risorse <cartella>] [--out <cartella>] [--lfs <cartella lfs.c>]
# Serve un compilatore C (cc). Niente altro da installare.
import argparse
import hashlib
import os
import re
import subprocess
import sys

QUI = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, QUI)
import mkrisorse
import verificarisorse

ROOT = os.path.dirname(QUI)
RADICE = os.path.dirname(ROOT)
BLOCCO = 4096
BLOCCHI_PARTIZIONE = 160            # spiffs 0xA0000 / 4096 (idf/partitions.csv)
MARGINE_BLOCCHI = 8                 # blocchi liberi lasciati nell'immagine corta (servono a lfs_fs_grow per aggiornare il superblocco)
ELENCO = 'SHA256SUMS'
RIGA_OK = re.compile(r'^([0-9a-f]{64})  ([a-z0-9._-]{1,63})$')
FILE_ATTESI = ['comuni.json.gz', 'html.vrs', 'licenza-apache2.txt.gz', 'licenza-bsd3.txt.gz',
               'licenza-mit.txt.gz', 'manuale.txt', 'radio.txt']


def errore(testo):
    sys.exit('ERRORE: ' + testo)


def leggi_elenco(cartella):
    """Dizionario nome -> sha256 letto da SHA256SUMS (formato di sha256sum)."""
    percorso = os.path.join(cartella, ELENCO)
    if not os.path.isfile(percorso):
        errore('manca %s (lancia tools/mkrisorse.py)' % percorso)
    voci = {}
    for riga in open(percorso, encoding='ascii').read().split('\n'):
        if not riga:
            continue
        m = RIGA_OK.match(riga)
        if not m:
            errore('riga non valida in %s: %r' % (ELENCO, riga))
        if m.group(2) in voci:
            errore('%s: nome doppio %s' % (ELENCO, m.group(2)))
        voci[m.group(2)] = m.group(1)
    return voci


def controlla_risorse(cartella):
    """Ferma tutto se i file non sono quelli attesi (elenco, SHA-256, html.vrs valido)."""
    voci = leggi_elenco(cartella)
    if sorted(voci) != sorted(FILE_ATTESI):
        errore('i file in %s sono %s, attesi %s' % (ELENCO, sorted(voci), sorted(FILE_ATTESI)))
    presenti = sorted(os.listdir(cartella))
    if presenti != sorted(FILE_ATTESI + [ELENCO]):
        errore('nella cartella %s ci sono file diversi dal previsto: %s' % (cartella, presenti))
    for nome in FILE_ATTESI:
        dati = open(os.path.join(cartella, nome), 'rb').read()
        if not dati:
            errore('file vuoto: ' + nome)
        if hashlib.sha256(dati).hexdigest() != voci[nome]:
            errore('lo SHA-256 di %s non coincide con %s' % (nome, ELENCO))
    try:
        nome, _ = verificarisorse.verifica(open(os.path.join(cartella, mkrisorse.HTML_VRS), 'rb').read())
    except ValueError as e:
        errore('html.vrs non valido: %s' % e)
    if nome != 'html':
        errore('html.vrs contiene la risorsa %r' % nome)
    return len(voci)


def compila(lfs, lavoro):
    """Compila lfs_immagine.c con il lfs.c del firmware. Ritorna il percorso del programma."""
    for f in ('lfs.c', 'lfs_util.c', 'lfs.h'):
        if not os.path.isfile(os.path.join(lfs, f)):
            errore('manca %s in %s' % (f, lfs))
    os.makedirs(lavoro, exist_ok=True)
    exe = os.path.join(lavoro, 'lfs_immagine')
    cmd = ['cc', '-std=c99', '-O1', '-Wall', '-DLFS_NO_DEBUG', '-DLFS_NO_WARN', '-DLFS_NO_ERROR',
           '-I' + lfs, '-I' + QUI, '-o', exe,
           os.path.join(QUI, 'lfs_immagine.c'), os.path.join(lfs, 'lfs.c'), os.path.join(lfs, 'lfs_util.c')]
    r = subprocess.run(cmd, capture_output=True, text=True)
    if r.returncode != 0:
        errore('compilazione fallita:\n' + r.stderr)
    return exe


def crea(exe, blocchi, uscita, file):
    r = subprocess.run([exe, str(blocchi), uscita] + file, capture_output=True, text=True)
    if r.returncode != 0:
        return None, r.stderr.strip()
    m = re.search(r'usati (\d+)', r.stdout)
    return int(m.group(1)), ''


def versione_firmware():
    t = open(os.path.join(ROOT, 'VesevOS', 'src', 'core', 'vos_common.h'), encoding='ascii').read()
    return re.search(r'#define\s+VOS_VERSION\s+"([^"]+)"', t).group(1)


def main():
    ap = argparse.ArgumentParser(description='Crea l\'immagine LittleFS corta dei file di /usr/share/vesevos')
    ap.add_argument('--risorse', default=os.path.join(RADICE, 'build', 'risorse', 'usr-share-vesevos'))
    ap.add_argument('--out', default=os.path.join(RADICE, 'build', 'risorse', 'immagine'))
    ap.add_argument('--lfs', default=os.path.join(RADICE, 'idf', 'managed_components', 'joltwallet__littlefs', 'src', 'littlefs'))
    a = ap.parse_args()

    controlla_risorse(a.risorse)
    file = [os.path.join(a.risorse, n) for n in sorted(FILE_ATTESI + [ELENCO])]
    exe = compila(a.lfs, os.path.join(a.out, 'lavoro'))
    os.makedirs(a.out, exist_ok=True)

    prova = os.path.join(a.out, 'lavoro', 'prova.bin')
    usati, motivo = crea(exe, BLOCCHI_PARTIZIONE, prova, file)
    if usati is None:
        errore('i file non entrano nemmeno nella partizione intera: ' + motivo)
    blocchi = usati + MARGINE_BLOCCHI
    nome = 'VesevOS-Risorse-%s.bin' % versione_firmware()
    uscita = os.path.join(a.out, nome)
    usati2, motivo = crea(exe, blocchi, uscita, file)
    if usati2 is None:
        errore('immagine da %d blocchi non riuscita: %s' % (blocchi, motivo))
    dati = open(uscita, 'rb').read()
    if len(dati) != blocchi * BLOCCO or len(dati) > BLOCCHI_PARTIZIONE * BLOCCO:
        errore('dimensione inattesa: %d byte' % len(dati))
    sha = hashlib.sha256(dati).hexdigest()
    open(uscita + '.sha256', 'w', encoding='ascii', newline='\n').write('%s  %s\n' % (sha, nome))
    print('ok %s: %d byte = %d blocchi da %d (usati %d + margine %d, partizione %d)' % (uscita, len(dati), blocchi, BLOCCO, usati2, MARGINE_BLOCCHI, BLOCCHI_PARTIZIONE))
    print('ok %d file piu %s (controllati contro %s)' % (len(FILE_ATTESI), ELENCO, ELENCO))
    print('ok SHA-256 %s' % sha)


if __name__ == '__main__':
    main()
