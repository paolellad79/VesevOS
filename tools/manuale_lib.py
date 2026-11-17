# -*- coding: utf-8 -*-
# SPDX-License-Identifier: GPL-3.0-or-later
# Manuale dei comandi: legge il sorgente tools/data/manuale_src.txt e lo divide in voci BREVI (restano nel file del manuale sulla scheda,
# /usr/share/vesevos/manuale.txt) e voci LUNGHE (oltre SOGLIA byte: fuori dal firmware, in sito-github/aiuto/<comando>.md).
# Usato da tools/mkrisorse.py (genera i file) e da tools/mklang.py (le frasi brevi sono le chiavi da tradurre).
#
# Formato del sorgente (ASCII, italiano):
#   # ...              commento
#   @<comando>         inizia una voce
#   <testo>            riga di descrizione: ogni riga e una frase tradotta dal firmware con tr()
#   $ <esempio>        esempio di comando (non si traduce)
# Dimensione di una voce = byte UTF-8 delle righe di descrizione unite da un a-capo (gli esempi non contano).
import os

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SORGENTE = os.path.join(ROOT, 'tools', 'data', 'manuale_src.txt')
SOGLIA = 300            # byte: oltre questa dimensione l'aiuto e lungo e va su GitHub
URL_AIUTO = 'https://github.com/paolellad79/VesevOS/blob/main/aiuto/'   # + <comando>.md (deve coincidere con VOS_AIUTO_URL nel firmware)


def leggi(percorso=SORGENTE):
    """Elenco di voci {nome, righe, esempi}. Errore chiaro se il file non e ben fatto."""
    voci = []
    for n, riga in enumerate(open(percorso, encoding='ascii').read().split('\n'), 1):
        if not riga.strip() or riga.startswith('#'):
            continue
        if riga.startswith('@'):
            nome = riga[1:].strip()
            if not nome or ' ' in nome or any(v['nome'] == nome for v in voci):
                raise SystemExit('manuale_src.txt riga %d: nome di voce non valido o doppio: %r' % (n, nome))
            voci.append({'nome': nome, 'righe': [], 'esempi': []})
        elif not voci:
            raise SystemExit('manuale_src.txt riga %d: testo prima della prima voce' % n)
        elif riga.startswith('$ '):
            voci[-1]['esempi'].append(riga[2:])
        else:
            voci[-1]['righe'].append(riga)
    return voci


def dimensione(voce):
    return len('\n'.join(voce['righe']).encode('utf-8'))


def lunga(voce):
    return dimensione(voce) > SOGLIA


def righe_brevi(voci):
    """Frasi italiane delle voci brevi: sono le chiavi che il firmware cerca con tr()."""
    return [r for v in voci if not lunga(v) for r in v['righe']]


def testo_manuale(voci, versione):
    """Contenuto del file manuale.txt: voci brevi con le loro righe, voci lunghe solo con '>' (rimando) e gli esempi."""
    out = ['# VesevOS: manuale dei comandi (generato da tools/mkrisorse.py, non modificare a mano)',
           '# versione=%s' % versione]
    for v in voci:
        out.append('@' + v['nome'])
        if lunga(v):
            out.append('>')
        else:
            out.extend(v['righe'])
        out.extend('$ ' + e for e in v['esempi'])
    return '\n'.join(out) + '\n'


def testo_md(voce):
    """Pagina .md di una voce lunga: tutto il testo di oggi, senza perdere niente."""
    out = ['# Aiuto del comando `%s`' % voce['nome'], '']
    for r in voce['righe']:
        out.extend([r, ''])
    if voce['esempi']:
        out.extend(['## Esempi', '', '```'])
        out.extend(voce['esempi'])
        out.extend(['```', ''])
    return '\n'.join(out)


def html_breve(voci):
    """Manuale breve per la pagina (tab Aiuto): per ogni voce breve un titolo con il nome e una riga <p> per frase.
    Le frasi sono le stesse di manuale.txt: la pagina le traduce con il suo dizionario (stesse chiavi del firmware). Gli esempi non entrano."""
    import html
    out = []
    for v in voci:
        if lunga(v):
            continue
        out.append('  <h5 data-nome="%s"></h5>' % html.escape(v['nome']))   # il nome lo scrive il CSS (.hlp h5::before): non e testo da tradurre
        out.extend('  <p>%s</p>' % html.escape(r, quote=False) for r in v['righe'])
    return '\n'.join(out)
