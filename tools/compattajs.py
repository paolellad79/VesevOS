#!/usr/bin/env python3
# Toglie commenti e spazi inutili dal JavaScript della pagina (passo 1 del riordino della pagina).
# La usa tools/mkpage.py prima del gzip: web/index.html e web/src restano leggibili.
#
# Regole, tutte prudenti:
#   - stringhe, stringhe con apici inversi ed espressioni regolari restano identiche, byte per byte;
#   - nessun nome cambia: i gestori onclick della pagina li usano;
#   - un a capo si toglie solo dopo ; { ( [ , oppure prima di } ) ]: altrove puo chiudere un'istruzione e resta;
#   - uno spazio resta se, togliendolo, due token si unirebbero; due token gia attaccati restano attaccati;
#   - l'HTML, il CSS e i gestori scritti nell'HTML non si toccano.
# Se una barra e dubbia (divisione o espressione regolare) il lavoro si ferma con un errore: il tokenizzatore di
# controllojs.py la leggerebbe come divisione, e dentro un'espressione regolare gli spazi contano.
#
# Prova da riga di comando: python3 tools/compattajs.py <pagina.html> <uscita.html>
# NON copre: il senso del codice. Il confronto finale vede solo che i token sono gli stessi e che gli a capo tolti sono
# quelli ammessi; lo stesso tokenizzatore fa da prova e da lavoro, quindi un suo errore di lettura non si vedrebbe.
import re
import sys

from controllojs import APERTE, CHIUSE, ErroreLettura, fine_token, token_di

SENZA_ACAPO_DOPO = {';', '{', '(', '[', ','}
SENZA_ACAPO_PRIMA = {'}', ')', ']'}
# Coppie che fuori da una stringa cambiano senso: commenti e commenti in stile HTML.
COPPIE_VIETATE = {'//', '/*', '<!', '->'}
BLOCCHI = ('if', 'while', 'for', 'with')


def parola(c):
    """Il carattere puo stare dentro un nome o un numero?"""
    return c.isalnum() or c in '_$\\' or ord(c) > 127


def serve_spazio(prec, t):
    """Tra i due token c'era uno spazio: serve ancora?"""
    a = prec.testo[-1]
    b = t.testo[0]
    if parola(a) and parola(b):
        return True
    if a + b in COPPIE_VIETATE:
        return True
    # La lettura dei due token attaccati deve fermarsi dove finisce il primo.
    return fine_token(prec.testo + t.testo, 0, 'x') != len(prec.testo)


def acapo_si_toglie(prec, t):
    return prec.testo in SENZA_ACAPO_DOPO or t.testo in SENZA_ACAPO_PRIMA


def separatore(prec, t):
    if t.acapo:
        return '' if acapo_si_toglie(prec, t) else '\n'
    if prec.inizio + len(prec.testo) == t.inizio:
        return ''
    return ' ' if serve_spazio(prec, t) else ''


def controlla_barre(toks):
    """Si ferma se una barra segue una chiusa dopo la quale potrebbe aprire un'espressione regolare."""
    pila = []
    for k, t in enumerate(toks):
        if t.testo in APERTE:
            pila.append(k)
        elif t.testo in CHIUSE and pila:
            apre = pila.pop()
            testa = toks[apre - 1].testo if apre else ''
            dopo = toks[k + 1].testo if k + 1 < len(toks) else ''
            dubbio = t.testo == '}' or (t.testo == ')' and testa in BLOCCHI)
            if dubbio and dopo.startswith('/'):
                raise ErroreLettura('barra dubbia alla riga %d del codice JS' % t.riga)


def controlla_uguale(toks, piccolo):
    """Stessi token, e ogni a capo tolto deve essere tra quelli ammessi."""
    nuovi = token_di(piccolo)
    if [t.testo for t in toks] != [t.testo for t in nuovi]:
        raise ErroreLettura('il codice compattato ha token diversi')
    for k in range(1, len(toks)):
        if toks[k].acapo and not nuovi[k].acapo and not acapo_si_toglie(toks[k - 1], toks[k]):
            raise ErroreLettura('a capo perso alla riga %d del codice JS' % toks[k].riga)
        if nuovi[k].acapo and not toks[k].acapo:
            raise ErroreLettura('a capo aggiunto alla riga %d del codice JS' % toks[k].riga)


def compatta_js(js):
    toks = token_di(js)
    controlla_barre(toks)
    pezzi = []
    for k, t in enumerate(toks):
        if k:
            pezzi.append(separatore(toks[k - 1], t))
        pezzi.append(t.testo)
    piccolo = ''.join(pezzi)
    controlla_uguale(toks, piccolo)
    return piccolo


def compatta_pagina(html):
    """Torna la pagina con il JS compattato; il resto del testo non cambia."""
    def blocco(m):
        return m.group(1) + compatta_js(m.group(2)) + m.group(3)

    return re.sub(r'(<script[^>]*>)(.*?)(</script>)', blocco, html, flags=re.S)


def main():
    if len(sys.argv) != 3:
        print('uso: compattajs.py <pagina.html> <uscita.html>')
        return 2
    try:
        html = open(sys.argv[1], encoding='utf-8').read()
        uscita = compatta_pagina(html)
    except (OSError, ErroreLettura) as e:
        print('ERRORE: %s' % e)
        return 1
    open(sys.argv[2], 'w', encoding='utf-8').write(uscita)
    print('ok: %d byte, dopo %d byte' % (len(html.encode('utf-8')), len(uscita.encode('utf-8'))))
    return 0


if __name__ == '__main__':
    sys.exit(main())
