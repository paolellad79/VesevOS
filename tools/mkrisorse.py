#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# SPDX-License-Identifier: GPL-3.0-or-later
# Genera i file di risorse VesevOS (pagina .vrs, dati comuni, licenze minori, manuale, tabella radio, SHA256SUMS).
# Nessun header per il firmware: dal 16/11/2026 il firmware non ha un id delle risorse.
# Piani: docs/progetto/COSE-DA-FARE.md#spostare-pagina-su-file-2026-11-15 e #risorse-file-singoli-2026-11-16.
#
# FORMATO .vrs (versione 1). Intestazione fissa di 96 byte, poi i dati. Numeri a 32 bit in little endian.
#   byte  0-3   marchio "VOSR"
#   byte  4     versione del formato (1)
#   byte  5     codifica dei dati: 0 = grezzi, 1 = gzip
#   byte  6-7   zero (riservati)
#   byte  8-23  identita del file: 16 cifre esadecimali minuscole = primi 16 caratteri dello SHA-256 dei dati (il firmware controlla solo che siano scritte bene)
#   byte 24-55  nome della risorsa: ASCII a-z 0-9 - _ , da 1 a 31 caratteri, completato con zero (esempio "html")
#   byte 56-59  lunghezza dei dati in byte (non zero)
#   byte 60-63  zero (riservati)
#   byte 64-95  SHA-256 dei dati (32 byte)
#   byte 96...  dati: esattamente "lunghezza" byte; il file e lungo 96 + lunghezza, non un byte di piu o di meno
#
# FILE SULLA SCHEDA (decisione dell'utente 16/11/2026): una sola cartella /usr/share/vesevos/ con
#   html.vrs               la pagina (formato .vrs qui sopra, risorsa "html", gzip)
#   comuni.json.gz         dati comuni (paesi, regole, elenco software): gzip, servito da /api/common
#   licenza-apache2.txt.gz, licenza-mit.txt.gz, licenza-bsd3.txt.gz   licenze minori (gzip), servite da /api/license
#   manuale.txt            manuale dei comandi: SOLO le voci brevi (<= 300 byte), per le lunghe un rimando (tools/manuale_lib.py)
#   radio.txt              tabella radio dei paesi: righe "CC canali dBm"
#   SHA256SUMS             elenco degli SHA-256 (formato di sha256sum: "<64 cifre>  <nome>"), controllabile con "sha256sum -c"
# NESSUNA ancora al firmware (decisione dell'utente 16/11/2026): il firmware controlla SHA256SUMS per formato (e nomi conosciuti) e ogni file contro la sua riga;
# html.vrs ha in piu lo SHA-256 dei dati nell'intestazione. Chi sostituisce elenco e file e un Admin (nessuna firma: progetto open source).
# Escono anche gli aiuti lunghi del manuale: sito-github/aiuto/<comando>.md (li pubblica l'utente su GitHub).
# Ordine di lavoro: mkweb, mklang, mkcommon, mkpage, mklicense, POI questo strumento (legge i dati grezzi in ../build/risorse/origine).
#
# Uso: python3 tools/mkrisorse.py [--out <cartella>] [--origine <cartella>] [--aiuto <cartella>]
#      --out     dove scrivere i file (predefinita: ../build/risorse/usr-share-vesevos, come stanno sulla scheda)
#      --origine dove mkpage.py, mkcommon.py e mklicense.py hanno scritto i dati grezzi (predefinita: ../build/risorse/origine)
#      --aiuto   dove scrivere gli aiuti lunghi (predefinita: ../sito-github/aiuto)
import argparse
import hashlib
import os
import re
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import manuale_lib
from versioni_risorse import versione_risorsa

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, 'VesevOS', 'src')

MARCHIO = b'VOSR'
VERSIONE_FORMATO = 1
INTEST = 96
CODIFICA_GREZZI = 0
CODIFICA_GZIP = 1
NOME_OK = re.compile(r'^[a-z0-9_-]{1,31}$')


def leggi_array(testo, nome_array):
    """Byte di 'static const uint8_t NOME[] PROGMEM = { 0x.., ... };' (solo numeri 0x..)."""
    m = re.search(r'static const uint8_t %s\[\] PROGMEM = \{(.*?)\};' % re.escape(nome_array), testo, re.S)
    if not m:
        raise SystemExit('ERRORE: array %s non trovato' % nome_array)
    return bytes(int(x, 16) for x in re.findall(r'0x([0-9a-fA-F]{2})', m.group(1)))


def leggi_numero(testo, nome):
    m = re.search(r'%s\s*=\s*(\d+);' % re.escape(nome), testo)
    if not m:
        raise SystemExit('ERRORE: costante %s non trovata' % nome)
    return int(m.group(1))


HTML_VRS = 'html.vrs'
FILE_SEMPLICI = ['comuni.json.gz', 'licenza-apache2.txt.gz', 'licenza-bsd3.txt.gz', 'licenza-mit.txt.gz', 'radio.txt']   # copiati da origine


def leggi_origine(origine, nome):
    percorso = os.path.join(origine, nome)
    if not os.path.isfile(percorso):
        raise SystemExit('ERRORE: manca %s (lancia prima mkpage.py, mkcommon.py e mklicense.py)' % percorso)
    return open(percorso, 'rb').read()


def raccogli_file(origine):
    """Dizionario nome -> byte dei file che NON sono la pagina (ordine alfabetico a carico di chi lo usa)."""
    file = {nome: leggi_origine(origine, nome) for nome in FILE_SEMPLICI}
    voci = manuale_lib.leggi()
    file['manuale.txt'] = manuale_lib.testo_manuale(voci, versione_risorsa('manuale')).encode('ascii')
    for nome, dati in file.items():
        if not dati:
            raise SystemExit('ERRORE: file %s vuoto' % nome)
    return file, voci


def pagina_gz(origine):
    dati = leggi_origine(origine, 'pagina.gz')
    if dati[:2] != b'\x1f\x8b':
        raise SystemExit('ERRORE: pagina.gz non e un file gzip (rigenera con mkpage.py)')
    return dati


def righe_elenco(file):
    """Righe di SHA256SUMS dei file semplici, in ordine di nome (senza html.vrs)."""
    return [(nome, hashlib.sha256(file[nome]).hexdigest()) for nome in sorted(file)]


def testo_elenco(file, vrs):
    """SHA256SUMS completo: tutti i file in ordine di nome, html.vrs compreso ."""
    righe = dict(righe_elenco(file))
    righe[HTML_VRS] = hashlib.sha256(vrs).hexdigest()
    return ''.join('%s  %s\n' % (righe[n], n) for n in sorted(righe))


def costruisci(nome, codifica, dati):
    if not NOME_OK.match(nome):
        raise SystemExit('ERRORE: nome risorsa non valido: %r' % nome)
    if not dati:
        raise SystemExit('ERRORE: risorsa %s vuota' % nome)
    idr = hashlib.sha256(dati).hexdigest()[:16]     # identita = prime 16 cifre dello SHA-256 dei dati
    intest = (MARCHIO + struct.pack('<BBH', VERSIONE_FORMATO, codifica, 0) + idr.encode('ascii')
              + nome.encode('ascii').ljust(32, b'\0') + struct.pack('<II', len(dati), 0) + hashlib.sha256(dati).digest())
    assert len(intest) == INTEST
    return intest + dati


def main():
    ap = argparse.ArgumentParser(description='Genera i file di /usr/share/vesevos, gli aiuti lunghi ')
    ap.add_argument('--out', default=os.path.join(os.path.dirname(ROOT), 'build', 'risorse', 'usr-share-vesevos'))
    ap.add_argument('--origine', default=os.path.join(os.path.dirname(ROOT), 'build', 'risorse', 'origine'))
    ap.add_argument('--aiuto', default=os.path.join(os.path.dirname(ROOT), 'sito-github', 'aiuto'))
    a = ap.parse_args()
    file, voci = raccogli_file(a.origine)
    os.makedirs(a.out, exist_ok=True)
    vrs = costruisci('html', CODIFICA_GZIP, pagina_gz(a.origine))
    scritti = dict(file)
    scritti[HTML_VRS] = vrs
    for nome, dati in sorted(scritti.items()):
        open(os.path.join(a.out, nome), 'wb').write(dati)
        print('ok %s: %d byte' % (os.path.join(a.out, nome), len(dati)))
    elenco = testo_elenco(file, vrs)
    open(os.path.join(a.out, 'SHA256SUMS'), 'w', encoding='ascii', newline='\n').write(elenco)
    print('ok SHA256SUMS: %d byte, %d file' % (len(elenco), elenco.count('\n')))
    os.makedirs(a.aiuto, exist_ok=True)
    for nome in os.listdir(a.aiuto):                                   # gli aiuti che non sono piu lunghi non devono restare
        if nome.endswith('.md') and nome[:-3] not in [v['nome'] for v in voci if manuale_lib.lunga(v)]:
            os.remove(os.path.join(a.aiuto, nome))
    for v in voci:
        if manuale_lib.lunga(v):
            open(os.path.join(a.aiuto, v['nome'] + '.md'), 'w', encoding='ascii', newline='\n').write(manuale_lib.testo_md(v))
    print('ok aiuti lunghi: %d in %s (voci brevi nel manuale: %d)' % (sum(1 for v in voci if manuale_lib.lunga(v)), a.aiuto, sum(1 for v in voci if not manuale_lib.lunga(v))))


if __name__ == '__main__':
    sys.exit(main())
