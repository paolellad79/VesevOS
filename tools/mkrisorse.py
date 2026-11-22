#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# SPDX-License-Identifier: GPL-3.0-or-later
# Genera i file di risorse VesevOS (pagina .vrs, dati comuni, licenze minori, manuale, tabella radio, SHA256SUMS).
# Nessun header per il firmware: dal 16/11/2026 il firmware non ha un id delle risorse.
# Piani: docs/progetto/COSE-DA-FARE.md#spostare-pagina-su-file-2026-11-15 e #risorse-file-singoli-2026-11-16.
#
# FORMATO .vrs / .vgz (contenitore "VOSR"). Intestazione fissa di 96 byte, poi i dati. Numeri a 32 bit in little endian.
#   byte  0-3   marchio "VOSR"
#   byte  4     versione del FORMATO: 1 = file vecchi (byte 6-7 e 60-63 a zero), 2 = file con versione e dipendenza (quelli che scriviamo oggi)
#   byte  5     codifica dei dati: 0 = grezzi, 1 = gzip
#   byte  6     formato 2: versione della risorsa, parte maggiore (0-255); formato 1: zero
#   byte  7     formato 2: versione della risorsa, parte minore (0-255); formato 1: zero.  Versione intera N = N.0
#   byte  8-23  identita del file: 16 cifre esadecimali minuscole = primi 16 caratteri dello SHA-256 dei dati (il firmware controlla solo che siano scritte bene)
#   byte 24-55  nome della risorsa: ASCII a-z 0-9 - _ , da 1 a 31 caratteri, completato con zero (esempio "html", "comuni"); niente punti
#   byte 56-59  lunghezza dei dati in byte (non zero)
#   byte 60-63  formato 2: DIPENDE, firmware minimo = 4 byte maggiore, minore, patch, lettera (0 = nessuna, 1 = a, 4 = d): 1.8.30 = 01 08 1E 00;
#               0 = nessuna dipendenza. Formato 1: zero
#   byte 64-95  SHA-256 dei dati (32 byte)
#   byte 96...  dati: esattamente "lunghezza" byte; il file e lungo 96 + lunghezza, non un byte di piu o di meno
# .vgz = stesso contenitore con codifica 1 (gzip): per i file .gz che non hanno posto per l'intestazione (comuni, licenze).
# I file nuovi (formato 2, .vgz) si caricano solo con firmware 1.8.30 o piu nuovo: il 1.8.29d li rifiuta (controlla i byte 6-7 e 60-63) e lascia intatto
# il file vecchio. Il firmware nuovo accetta anche il formato 1. Una risorsa che richiede un firmware piu nuovo di quello in uso e rifiutata (RIS_DIPENDE).
# FILE DI TESTO: PRIMA riga "# sha256=<64 cifre minuscole>" (74 byte con l'a-capo) = SHA-256 di TUTTO il testo che segue quella riga (a-capo "\n", compresi
# "# versione=N" e "# dipende=firmware>=1.8.30", che vengono subito dopo). Il file garantisce se stesso: la scheda controlla l'impronta e aggiorna da sola la
# sua riga di SHA256SUMS. Un testo senza la riga (vecchio) si carica solo se SHA256SUMS ne conferma l'impronta intera.
# IMPRONTA INTERNA DEI BINARI: i file VOSR formato 2 l'hanno gia (byte 64-95 = SHA-256 dei dati): non esiste un formato 3. Anche per loro la scheda
# aggiorna da sola la propria riga di SHA256SUMS (che resta l'elenco dei file INTERI: nessuna impronta dentro SHA256SUMS stesso).
# Versioni e firmware minimo: UN solo posto, tools/data/versioni_risorse.txt.
# Nome distribuito (copia in ../consegna/risorse): <nome>-<versione>.<estensione>, per esempio html-5.vrs; sulla scheda resta il nome corto.
#
# FILE SULLA SCHEDA (decisione dell'utente 16/11/2026): una sola cartella /usr/share/vesevos/ con
#   html.vrs               la pagina (contenitore qui sopra, risorsa "html", gzip)
#   comuni.vgz             dati comuni (paesi, regole, elenco software): contenitore VOSR con gzip, servito da /api/common
#   licenza-apache2.vgz, licenza-mit.vgz, licenza-bsd3.vgz   licenze minori (VOSR con gzip), servite da /api/license
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
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import manuale_lib
from versioni_risorse import versione_risorsa, firmware_minimo, impacchetta_firmware, versione_due_byte

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, 'VesevOS', 'src')

MARCHIO = b'VOSR'
VERSIONE_FORMATO = 2
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
# File .vgz: nome sulla scheda -> (nome interno, chiave della versione, file grezzo in origine)
FILE_VGZ = {
    'comuni.vgz': ('comuni', 'comuni', 'comuni.json.gz'),
    'licenza-apache2.vgz': ('licenza-apache2', 'licenza-apache2', 'licenza-apache2.txt.gz'),
    'licenza-bsd3.vgz': ('licenza-bsd3', 'licenza-bsd3', 'licenza-bsd3.txt.gz'),
    'licenza-mit.vgz': ('licenza-mit', 'licenza-mit', 'licenza-mit.txt.gz'),
}
FILE_TESTO = {'radio.txt': 'radio'}        # copiati da origine (la riga di versione la scrive mkcommon.py); manuale.txt la scrive manuale_lib
CHIAVE_VERSIONE = {HTML_VRS: 'pagina', 'manuale.txt': 'manuale', 'radio.txt': 'radio'}
CHIAVE_VERSIONE.update({n: v[1] for n, v in FILE_VGZ.items()})


ETICHETTA_SHA = b'# sha256='


def con_impronta(testo):
    """Un file di testo con la riga '# sha256=<64 cifre>' in cima: SHA-256 di tutto il testo che segue quella riga. Se la riga c'e gia la rifa."""
    if testo.startswith(ETICHETTA_SHA):
        testo = testo[testo.index(b'\n') + 1:]
    if b'\r' in testo:
        raise SystemExit('ERRORE: il testo contiene a-capo \\r (solo \\n)')
    return ETICHETTA_SHA + hashlib.sha256(testo).hexdigest().encode('ascii') + b'\n' + testo


def leggi_origine(origine, nome):
    percorso = os.path.join(origine, nome)
    if not os.path.isfile(percorso):
        raise SystemExit('ERRORE: manca %s (lancia prima mkpage.py, mkcommon.py e mklicense.py)' % percorso)
    return open(percorso, 'rb').read()


def raccogli_file(origine):
    """Dizionario nome sulla scheda -> byte dei file che NON sono la pagina (ordine alfabetico a carico di chi lo usa)."""
    file = {nome: con_impronta(leggi_origine(origine, nome)) for nome in FILE_TESTO}
    for nome, (interno, chiave, grezzo) in FILE_VGZ.items():
        gz = leggi_origine(origine, grezzo)
        if gz[:2] != b'\x1f\x8b':
            raise SystemExit('ERRORE: %s non e un file gzip' % grezzo)
        file[nome] = costruisci(interno, CODIFICA_GZIP, gz, versione_risorsa(chiave))
    voci = manuale_lib.leggi()
    file['manuale.txt'] = con_impronta(manuale_lib.testo_manuale(voci, versione_risorsa('manuale')).encode('ascii'))
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


def costruisci(nome, codifica, dati, versione):
    """Un file VOSR formato 2: intestazione di 96 byte con versione propria e firmware minimo, poi i dati."""
    if not NOME_OK.match(nome):
        raise SystemExit('ERRORE: nome risorsa non valido: %r' % nome)
    if not dati:
        raise SystemExit('ERRORE: risorsa %s vuota' % nome)
    maggiore, minore = versione_due_byte(versione)
    idr = hashlib.sha256(dati).hexdigest()[:16]     # identita = prime 16 cifre dello SHA-256 dei dati
    intest = (MARCHIO + struct.pack('<BBBB', VERSIONE_FORMATO, codifica, maggiore, minore) + idr.encode('ascii')
              + nome.encode('ascii').ljust(32, b'\0') + struct.pack('<I', len(dati)) + impacchetta_firmware(firmware_minimo())
              + hashlib.sha256(dati).digest())
    assert len(intest) == INTEST
    return intest + dati


def nome_lungo(nome, versione):
    """html.vrs + 5 -> html-5.vrs ; comuni.vgz + 1 -> comuni-1.vgz (regola nomi-dei-pacchetti.md)."""
    base, punto, est = nome.partition('.')
    return '%s-%s.%s' % (base, versione, est)


def chiave_ordine(nome):
    """Versione contenuta nel nome lungo, come tupla di numeri, per tenere le ultime tre."""
    m = re.match(r'^(.*)-(\d+(?:\.\d+)?)\.([a-z0-9]+)$', nome)
    if not m:
        return None
    return m.group(1), tuple(int(x) for x in m.group(2).split('.')), m.group(3)


def copia_consegna(scritti, cartella):
    """Copia con nome lungo in consegna/risorse; tiene le ultime tre versioni di ogni risorsa, il resto va in archivio/risorse.
    Stesso nome con contenuto diverso = errore (alza la versione in tools/data/versioni_risorse.txt). Ritorna il numero di errori."""
    archivio = os.path.join(os.path.dirname(cartella), 'archivio', 'risorse')
    os.makedirs(cartella, exist_ok=True)
    errori = 0
    for nome, dati in sorted(scritti.items()):
        if nome == 'SHA256SUMS':
            destinazione = os.path.join(cartella, nome)         # l'elenco non ha versione: e uno solo, quello dell'ultimo giro
        else:
            destinazione = os.path.join(cartella, nome_lungo(nome, versione_risorsa(CHIAVE_VERSIONE[nome])))
        if os.path.isfile(destinazione) and nome != 'SHA256SUMS' and open(destinazione, 'rb').read() != dati:
            print('ERRORE: %s esiste con contenuto diverso: alza la versione in tools/data/versioni_risorse.txt' % destinazione)
            errori += 1
            continue
        open(destinazione, 'wb').write(dati)
        print('ok consegna: %s' % destinazione)
    gruppi = {}
    for n in os.listdir(cartella):
        c = chiave_ordine(n)
        if c:
            gruppi.setdefault((c[0], c[2]), []).append((c[1], n))
    spostati = []
    for chiave, elenco in gruppi.items():
        for _, n in sorted(elenco)[:-3]:
            os.makedirs(archivio, exist_ok=True)
            os.replace(os.path.join(cartella, n), os.path.join(archivio, n))
            spostati.append('consegna/risorse/%s -> consegna/archivio/risorse/%s' % (n, n))
    if spostati:
        elenco_sp = os.path.join(os.path.dirname(cartella), 'archivio', 'SPOSTAMENTI-%s.txt' % time.strftime('%Y-%m-%d'))
        with open(elenco_sp, 'a', encoding='ascii') as f:
            f.write(''.join(r + '\n' for r in spostati))
        print('ok archivio: %d file spostati (vedi %s)' % (len(spostati), elenco_sp))
    return errori


def main():
    ap = argparse.ArgumentParser(description='Genera i file di /usr/share/vesevos, gli aiuti lunghi ')
    ap.add_argument('--out', default=os.path.join(os.path.dirname(ROOT), 'build', 'risorse', 'usr-share-vesevos'))
    ap.add_argument('--origine', default=os.path.join(os.path.dirname(ROOT), 'build', 'risorse', 'origine'))
    ap.add_argument('--consegna', default=os.path.join(os.path.dirname(ROOT), 'consegna', 'risorse'),
                    help='dove copiare i file con nome lungo (vuoto = non copiare)')
    ap.add_argument('--aiuto', default=os.path.join(os.path.dirname(ROOT), 'sito-github', 'aiuto'))
    a = ap.parse_args()
    file, voci = raccogli_file(a.origine)
    os.makedirs(a.out, exist_ok=True)
    vrs = costruisci('html', CODIFICA_GZIP, pagina_gz(a.origine), versione_risorsa('pagina'))
    scritti = dict(file)
    scritti[HTML_VRS] = vrs
    for nome, dati in sorted(scritti.items()):
        open(os.path.join(a.out, nome), 'wb').write(dati)
        print('ok %s: %d byte' % (os.path.join(a.out, nome), len(dati)))
    elenco = testo_elenco(file, vrs)
    open(os.path.join(a.out, 'SHA256SUMS'), 'w', encoding='ascii', newline='\n').write(elenco)
    print('ok SHA256SUMS: %d byte, %d file' % (len(elenco), elenco.count('\n')))
    errori = 0
    if a.consegna:
        scritti['SHA256SUMS'] = elenco.encode('ascii')
        errori = copia_consegna(scritti, a.consegna)
    os.makedirs(a.aiuto, exist_ok=True)
    for nome in os.listdir(a.aiuto):                                   # gli aiuti che non sono piu lunghi non devono restare
        if nome.endswith('.md') and nome[:-3] not in [v['nome'] for v in voci if manuale_lib.lunga(v)]:
            os.remove(os.path.join(a.aiuto, nome))
    for v in voci:
        if manuale_lib.lunga(v):
            open(os.path.join(a.aiuto, v['nome'] + '.md'), 'w', encoding='ascii', newline='\n').write(manuale_lib.testo_md(v))
    print('ok aiuti lunghi: %d in %s (voci brevi nel manuale: %d)' % (sum(1 for v in voci if manuale_lib.lunga(v)), a.aiuto, sum(1 for v in voci if not manuale_lib.lunga(v))))
    return 1 if errori else 0


if __name__ == '__main__':
    sys.exit(main())
