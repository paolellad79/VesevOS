#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# SPDX-License-Identifier: GPL-3.0-or-later
# Rilegge uno o piu file .vrs (formato descritto in tools/mkrisorse.py) e controlla: marchio, versione del formato, identita scritta bene,
# nome, lunghezza (il file e lungo esattamente 96 + lunghezza) e SHA-256 dei dati.
# Uso: python3 tools/verificarisorse.py file.vrs [file2.vrs ...]
#      python3 tools/verificarisorse.py --cartella <usr-share-vesevos>   (controlla tutta la cartella con SHA256SUMS)
# Nessun id del firmware (16/11/2026): l'identita in intestazione e solo un'informazione, qui si controlla che sia = prime 16 cifre dello SHA-256 dei dati.
# Esce con 0 se tutti i file sono buoni, con 1 al primo che non va (il motivo e scritto).
import argparse
import hashlib
import os
import re
import struct
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
INTEST = 96
NOME_OK = re.compile(r'^[a-z0-9_-]{1,31}$')
ID_OK = re.compile(r'^[0-9a-f]{16}$')


def verifica(dati):
    """Ritorna (nome, lunghezza) se il file e buono, altrimenti alza ValueError con il motivo."""
    if len(dati) < INTEST:
        raise ValueError('troncato: meno di %d byte' % INTEST)
    if dati[0:4] != b'VOSR':
        raise ValueError('marchio sbagliato')
    versione, codifica, ver_mag, ver_min = struct.unpack('<BBBB', dati[4:8])
    if versione not in (1, 2):
        raise ValueError('versione del formato %d non supportata' % versione)
    if codifica not in (0, 1) or (versione == 1 and (ver_mag or ver_min)):
        raise ValueError('codifica o byte riservati non validi')
    if versione == 2 and ver_mag == 0 and ver_min == 0:
        raise ValueError('formato 2 senza versione della risorsa')
    idr = dati[8:24].decode('ascii', 'replace')
    if not ID_OK.match(idr):
        raise ValueError('id non valido')
    if idr != hashlib.sha256(dati[INTEST:]).hexdigest()[:16]:
        raise ValueError('identita %s diversa dalle prime 16 cifre dello SHA-256 dei dati' % idr)
    nome_grezzo = dati[24:56]
    nome = nome_grezzo.rstrip(b'\0')
    if b'\0' in nome or nome_grezzo[len(nome):] != b'\0' * (32 - len(nome)):
        raise ValueError('nome non valido (zero in mezzo)')
    nome = nome.decode('ascii', 'replace')
    if not NOME_OK.match(nome):
        raise ValueError('nome non valido')
    lunghezza, dipende = struct.unpack('<I4s', dati[56:64])
    if lunghezza == 0 or (versione == 1 and dipende != b'\0\0\0\0'):
        raise ValueError('lunghezza zero o byte riservati non validi')
    if versione == 2 and dipende[3] > 26:
        raise ValueError('firmware minimo non valido (lettera)')
    if len(dati) < INTEST + lunghezza:
        raise ValueError('troncato: dati %d byte, ne servono %d' % (len(dati) - INTEST, lunghezza))
    if len(dati) > INTEST + lunghezza:
        raise ValueError('troppi byte dopo i dati')
    if hashlib.sha256(dati[INTEST:]).digest() != dati[64:96]:
        raise ValueError('SHA-256 dei dati diverso da quello scritto')
    return nome, lunghezza


def verifica_cartella(cartella):
    """SHA256SUMS: ogni riga ha il formato di sha256sum e il file giusto, non ci sono file in piu."""
    elenco = open(os.path.join(cartella, 'SHA256SUMS'), encoding='ascii').read()
    voci = {}
    for riga in elenco.split('\n')[:-1]:
        m = re.match(r'^([0-9a-f]{64})  ([A-Za-z0-9._-]{1,40})$', riga)
        if not m or m.group(2) in voci:
            print('RIFIUTATO riga di SHA256SUMS non valida o doppia: %r' % riga)
            return 1
        voci[m.group(2)] = m.group(1)
    presenti = set(os.listdir(cartella)) - {'SHA256SUMS'}
    if presenti != set(voci):
        print('RIFIUTATO file diversi dall\'elenco: %s' % sorted(presenti ^ set(voci)))
        return 1
    for nome, esa in voci.items():
        if hashlib.sha256(open(os.path.join(cartella, nome), 'rb').read()).hexdigest() != esa:
            print('RIFIUTATO %s: SHA-256 diverso dall\'elenco' % nome)
            return 1
    for nome in sorted(voci):
        dati = open(os.path.join(cartella, nome), 'rb').read()
        try:
            if nome.endswith(('.vrs', '.vgz')):
                interno, _ = verifica(dati)
                if nome.endswith('.vgz') and (dati[5] != 1 or dati[INTEST:INTEST + 2] != b'\x1f\x8b'):
                    raise ValueError('un .vgz deve avere codifica 1 e dati gzip')
                if dati[4] != 2:
                    raise ValueError('formato 1: manca versione e dipendenza')
            elif nome.endswith('.txt'):
                m = re.match(rb'^# sha256=([0-9a-f]{64})\n', dati)
                if not m or hashlib.sha256(dati[74:]).hexdigest() != m.group(1).decode():
                    raise ValueError('manca la prima riga "# sha256=" oppure l\'impronta non corrisponde al testo che la segue')
                testa = dati[:1024].decode('ascii', 'replace')
                if not re.search(r'^# versione=\d{1,3}(\.\d{1,3})?$', testa, re.M) or not re.search(r'^# dipende=firmware>=\d+\.\d+\.\d+[a-z]?$', testa, re.M):
                    raise ValueError('mancano le righe "# versione=" e "# dipende=" nei primi 1024 byte')
        except (ValueError, OSError) as e:
            print('RIFIUTATO %s: %s' % (nome, e))
            return 1
    print('ok cartella %s: %d file' % (cartella, len(voci)))
    return 0


def main():
    ap = argparse.ArgumentParser(description='Controlla file .vrs')
    ap.add_argument('--cartella', default=None, help='cartella /usr/share/vesevos generata da mkrisorse.py')
    ap.add_argument('file', nargs='*')
    a = ap.parse_args()
    if a.cartella:
        return verifica_cartella(a.cartella)
    if not a.file:
        ap.error('serve almeno un file oppure --cartella')
    for f in a.file:
        try:
            nome, lung = verifica(open(f, 'rb').read())
        except (ValueError, OSError) as e:
            print('RIFIUTATO %s: %s' % (f, e))
            return 1
        print('ok %s: risorsa "%s", %d byte di dati' % (f, nome, lung))
    return 0


if __name__ == '__main__':
    sys.exit(main())
