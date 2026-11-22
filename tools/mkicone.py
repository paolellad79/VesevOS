#!/usr/bin/env python3
# Copia nel firmware le tre icone PNG dell'app web (quelle del sito: sito-github/apple-touch-icon.png, icon-192.png, icon-512.png).
# Scrive VesevOS/src/interfaces/vos_icone_data.h (solo ASCII, byte in esadecimale). Le icone escono dalle rotte /apple-touch-icon.png, /icon-192.png e
# /icon-512.png (interfaces/vos_web_pwa.cpp). Controlla che siano PNG veri e delle misure giuste prima di scrivere.
# Uso: python3 tools/mkicone.py          (scrive il file)   |   python3 tools/mkicone.py --controlla   (esce con errore se il file e vecchio)
import os
import struct
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SITO = os.path.join(os.path.dirname(ROOT), 'sito-github')
DST = os.path.join(ROOT, 'VesevOS', 'src', 'interfaces', 'vos_icone_data.h')
ICONE = [('kIcona180', 'apple-touch-icon.png', 180), ('kIcona192', 'icon-192.png', 192), ('kIcona512', 'icon-512.png', 512)]
FIRMA = b'\x89PNG\r\n\x1a\n'

TESTA = '''// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_icone_data.h
// GENERATO da tools/mkicone.py dalle icone del sito: non modificare a mano. Le tre icone PNG dell'app web (rotte in interfaces/vos_web_pwa.cpp).
#pragma once
#include <stddef.h>
#include <stdint.h>
'''


def leggi(nome, lato):
    dati = open(os.path.join(SITO, nome), 'rb').read()
    if not dati.startswith(FIRMA):
        raise SystemExit('%s: non e un PNG' % nome)
    larg, alt = struct.unpack('>II', dati[16:24])
    if larg != lato or alt != lato:
        raise SystemExit('%s: misure %dx%d, servono %dx%d' % (nome, larg, alt, lato, lato))
    return dati


def genera():
    t = TESTA
    for sim, nome, lato in ICONE:
        dati = leggi(nome, lato)
        t += '\nstatic const size_t %sLen = %d;\nstatic const uint8_t %s[] = {\n' % (sim, len(dati), sim)
        for i in range(0, len(dati), 16):
            t += '  ' + ','.join('0x%02x' % b for b in dati[i:i + 16]) + ',\n'
        t += '};\n'
    return t


if __name__ == '__main__':
    nuovo = genera()
    if '--controlla' in sys.argv:
        vecchio = open(DST, encoding='ascii').read() if os.path.exists(DST) else ''
        if vecchio != nuovo:
            print('vos_icone_data.h e vecchio: python3 tools/mkicone.py')
            sys.exit(1)
        print('ok vos_icone_data.h aggiornato')
    else:
        open(DST, 'w', encoding='ascii').write(nuovo)
        print('ok vos_icone_data.h scritto: %d byte' % len(nuovo))
