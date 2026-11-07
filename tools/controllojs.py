#!/usr/bin/env python3
# Controlli sul codice JS della pagina, senza librerie esterne (passo 0 del riordino della pagina).
#
#   python3 tools/controllojs.py uguali <prima> <dopo>   stesse istruzioni nei due file?
#   python3 tools/controllojs.py doppi [<pagina>]        nomi globali dichiarati due volte?
#
# I file sono pagine .html (si leggono i blocchi <script>) oppure .js. Senza <pagina> vale web/index.html.
# Per un passo del riordino: copiare web/index.html fuori dal repo PRIMA, poi confrontare la copia con la pagina nuova.
#
# uguali: toglie commenti e spazi e confronta i pezzi di codice (token).
#   - stessa sequenza: esito ok, nulla e cambiato;
#   - istruzioni solo spostate: esito ok se restano nello stesso ordine quelle eseguite al caricamento
#     (le dichiarazioni di funzione valgono in tutto il blocco e si possono spostare);
#   - altrimenti esito KO, con le istruzioni diverse.
#   Confronta anche il resto della pagina (HTML e CSS) a parita di spazi.
# doppi: elenca i nomi globali (var, let, const, function fuori dalle funzioni) dichiarati piu volte.
#
# NON copre: i gestori scritti nell'HTML (onclick e simili) sono solo testo; i nomi globali creati senza dichiarazione;
# un a capo aggiunto dove cambia il senso del codice (esempio subito dopo return), perche gli a capo non sono token.
# Esce con 0 se il controllo passa, 1 se non passa, 2 se il file non si legge.
import os
import re
import sys
from collections import Counter

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PAGINA = os.path.join(ROOT, 'web', 'index.html')

# Parole dopo le quali una barra apre un'espressione regolare e non una divisione.
PRIMA_DI_REGEX = {'return', 'typeof', 'instanceof', 'in', 'of', 'new', 'delete', 'void', 'throw', 'case', 'do', 'else'}
# Parole chiave che non possono essere la fine di un'istruzione.
PAROLE_CHIAVE = PRIMA_DI_REGEX | {'var', 'let', 'const', 'function', 'if', 'for', 'while', 'switch', 'try', 'catch', 'finally'}
# Operatori di piu caratteri, dal piu lungo.
OPERATORI = ['>>>=', '===', '!==', '**=', '<<=', '>>=', '>>>', '...', '&&=', '||=', '??=', '=>', '==', '!=', '<=', '>=',
             '&&', '||', '??', '?.', '++', '--', '+=', '-=', '*=', '/=', '%=', '&=', '|=', '^=', '<<', '>>', '**']
APERTE = {'(': ')', '[': ']', '{': '}'}
CHIUSE = set(APERTE.values())
RE_NOME = re.compile(r'[A-Za-z_$][A-Za-z0-9_$]*')
RE_NUMERO = re.compile(r'0[xXbBoO][0-9a-fA-F]+|\d[\d.]*(?:[eE][+-]?\d+)?|\.\d+(?:[eE][+-]?\d+)?')


class ErroreLettura(Exception):
    pass


class Token:
    def __init__(self, testo, acapo, riga, inizio):
        self.testo = testo
        self.inizio = inizio  # posizione nel codice JS
        self.acapo = acapo    # c'era un a capo prima di questo token?
        self.riga = riga      # riga nel codice JS (dal blocco)


def fine_stringa(s, i):
    """Indice dopo la stringa tra apici che comincia in i."""
    q = s[i]
    j = i + 1
    while j < len(s):
        c = s[j]
        if c == '\\':
            j += 2
            continue
        if c == '\n':
            break
        if c == q:
            return j + 1
        j += 1
    raise ErroreLettura('stringa non chiusa alla posizione %d' % i)


def fine_modello(s, i):
    """Indice dopo la stringa con apici inversi che comincia in i, comprese le parti ${...}."""
    j = i + 1
    while j < len(s):
        c = s[j]
        if c == '\\':
            j += 2
            continue
        if c == '`':
            return j + 1
        if s.startswith('${', j):
            j = fine_parte(s, j + 2)
            continue
        j += 1
    raise ErroreLettura('stringa con apici inversi non chiusa alla posizione %d' % i)


def fine_parte(s, i):
    """Indice dopo la graffa che chiude una parte ${...}."""
    livello = 1
    j = i
    while j < len(s):
        c = s[j]
        if c in '\'"':
            j = fine_stringa(s, j)
            continue
        if c == '`':
            j = fine_modello(s, j)
            continue
        if c == '{':
            livello += 1
        if c == '}':
            livello -= 1
            if livello == 0:
                return j + 1
        j += 1
    raise ErroreLettura('parte ${...} non chiusa alla posizione %d' % i)


def fine_regex(s, i):
    """Indice dopo l'espressione regolare che comincia in i, opzioni comprese."""
    j = i + 1
    in_classe = False
    while j < len(s):
        c = s[j]
        if c == '\\':
            j += 2
            continue
        if c == '\n':
            break
        if c == '[':
            in_classe = True
        elif c == ']':
            in_classe = False
        elif c == '/' and not in_classe:
            j += 1
            while j < len(s) and s[j].isalpha():
                j += 1
            return j
        j += 1
    raise ErroreLettura('espressione regolare non chiusa alla posizione %d' % i)


def barra_apre_regex(ultimo):
    if ultimo is None:
        return True
    if ultimo in PRIMA_DI_REGEX:
        return True
    if RE_NOME.fullmatch(ultimo) or ultimo[0].isdigit() or ultimo[0] in '\'"`/':
        return False
    return ultimo not in CHIUSE


def token_di(s):
    """Divide il codice in token. I commenti e gli spazi spariscono."""
    out = []
    i = 0
    riga = 1
    acapo = False
    while i < len(s):
        c = s[i]
        if c in ' \t\r\n':
            if c == '\n':
                acapo = True
                riga += 1
            i += 1
            continue
        if s.startswith('//', i):
            fine = s.find('\n', i)
            i = len(s) if fine < 0 else fine
            continue
        if s.startswith('/*', i):
            fine = s.find('*/', i)
            if fine < 0:
                raise ErroreLettura('commento non chiuso alla riga %d' % riga)
            pezzo = s[i:fine + 2]
            if '\n' in pezzo:
                acapo = True
                riga += pezzo.count('\n')
            i = fine + 2
            continue
        fine = fine_token(s, i, out[-1].testo if out else None)
        testo = s[i:fine]
        out.append(Token(testo, acapo, riga, i))
        riga += testo.count('\n')
        acapo = False
        i = fine
    return out


def fine_token(s, i, ultimo):
    c = s[i]
    if c in '\'"':
        return fine_stringa(s, i)
    if c == '`':
        return fine_modello(s, i)
    if c == '/' and barra_apre_regex(ultimo):
        return fine_regex(s, i)
    m = RE_NOME.match(s, i) or RE_NUMERO.match(s, i)
    if m:
        return m.end()
    if s.startswith('?.', i) and s[i + 2:i + 3].isdigit():
        return i + 1          # a?.5:b e un'alternativa, non un accesso facoltativo
    for op in OPERATORI:
        if s.startswith(op, i):
            return i + len(op)
    return i + 1


def leggi(percorso):
    """Torna (codice JS, resto del file a parita di spazi)."""
    try:
        testo = open(percorso, encoding='utf-8', newline='').read()
    except OSError as e:
        raise ErroreLettura('non riesco a leggere %s: %s' % (percorso, e.strerror))
    if percorso.endswith('.js'):
        return testo, ''
    blocchi = re.findall(r'<script[^>]*>(.*?)</script>', testo, re.S)
    resto = re.sub(r'<script[^>]*>.*?</script>', '<script>', testo, flags=re.S)
    return '\n'.join(blocchi), ' '.join(resto.split())


def puo_finire(t):
    """Il token t puo chiudere un'istruzione senza punto e virgola?"""
    x = t.testo
    if x in PAROLE_CHIAVE:
        return False
    if x in CHIUSE or x in ('++', '--'):
        return True
    return x[0].isalnum() or x[0] in '_$\'"`/'


def puo_iniziare(t):
    """Il token t puo aprire un'istruzione dopo un a capo?"""
    x = t.testo
    return x[0].isalpha() or x[0] in '_$' or x in ('++', '--', '{')


def fine_automatica(prec, t):
    """L'a capo tra prec e t chiude l'istruzione?"""
    if not t.acapo or not puo_finire(prec) or not puo_iniziare(t):
        return False
    return t.testo not in ('in', 'of', 'instanceof')


def istruzioni(toks):
    """Divide i token nelle istruzioni di primo livello. Torna una lista di (tipo, testi); tipo 'f' = dichiarazione di funzione, 'c' = eseguita al caricamento."""
    out = []
    pezzo = []
    livello = 0

    def chiudi():
        if pezzo:
            tipo = 'f' if pezzo[0].testo == 'function' else 'c'
            out.append((tipo, tuple(x.testo for x in pezzo)))
            pezzo.clear()

    for k, t in enumerate(toks):
        if livello == 0 and pezzo and fine_automatica(pezzo[-1], t):
            chiudi()
        pezzo.append(t)
        if t.testo in APERTE:
            livello += 1
        elif t.testo in CHIUSE:
            livello -= 1
        if livello:
            continue
        if t.testo == ';':
            chiudi()
        elif t.testo == '}' and blocco_finito(pezzo, toks, k):
            chiudi()
    chiudi()
    return out


def blocco_finito(pezzo, toks, k):
    """Una graffa chiusa a livello zero chiude l'istruzione se questa e un blocco e non segue il suo seguito."""
    if pezzo[0].testo not in ('function', 'if', 'for', 'while', 'try', 'switch', 'do', '{'):
        return False
    dopo = toks[k + 1].testo if k + 1 < len(toks) else ''
    return dopo not in ('else', 'catch', 'finally', 'while')


def corpi_di_funzione(toks):
    """Indici delle graffe che aprono il corpo di una funzione."""
    corpi = set()
    for i, t in enumerate(toks):
        if t.testo == '=>' and i + 1 < len(toks) and toks[i + 1].testo == '{':
            corpi.add(i + 1)
        if t.testo != 'function':
            continue
        j = i + 1
        while toks[j].testo != '(':
            j += 1
        j = chiusura(toks, j)
        if toks[j + 1].testo == '{':
            corpi.add(j + 1)
    return corpi


def chiusura(toks, i):
    """Indice della parentesi che chiude quella aperta in i."""
    livello = 0
    for j in range(i, len(toks)):
        if toks[j].testo in APERTE:
            livello += 1
        elif toks[j].testo in CHIUSE:
            livello -= 1
            if livello == 0:
                return j
    raise ErroreLettura('parentesi non chiusa alla riga %d' % toks[i].riga)


def dichiarati(toks, i):
    """Nomi dichiarati dalla parola var/let/const in posizione i. Salta i valori iniziali."""
    nomi = []
    livello = 0
    atteso = True
    for j in range(i + 1, len(toks)):
        t = toks[j]
        if atteso and livello == 0 and RE_NOME.fullmatch(t.testo):
            nomi.append(t)
            atteso = False
            continue
        if t.testo in APERTE:
            livello += 1
        elif t.testo in CHIUSE:
            livello -= 1
            if livello < 0:
                break
        if livello:
            continue
        if t.testo == ';' or t.testo in ('in', 'of'):
            break
        if t.testo == ',':
            atteso = True
        elif fine_automatica(toks[j - 1], t):
            break
    return nomi


def nomi_globali(toks):
    """Torna {nome: [(tipo, riga), ...]} per i nomi dichiarati fuori dalle funzioni."""
    corpi = corpi_di_funzione(toks)
    pila = []
    trovati = {}

    def segna(nome, tipo):
        trovati.setdefault(nome.testo, []).append((tipo, nome.riga))

    for i, t in enumerate(toks):
        if t.testo == '{':
            pila.append(i in corpi)
        elif t.testo == '}':
            pila.pop()
        if any(pila):
            continue
        if t.testo in ('var', 'let', 'const'):
            for nome in dichiarati(toks, i):
                segna(nome, t.testo)
        elif t.testo == 'function' and e_dichiarazione(toks, i):
            segna(toks[i + 1], 'function')
    return trovati


def e_dichiarazione(toks, i):
    """Il 'function' in posizione i apre una dichiarazione con nome, non un'espressione?"""
    if not RE_NOME.fullmatch(toks[i + 1].testo):
        return False
    if i == 0 or toks[i].acapo:
        return True
    return toks[i - 1].testo in (';', '}', '{', ')')


def corto(testi):
    x = ' '.join(testi)
    return x if len(x) <= 110 else x[:110] + ' ...'


def elenca(titolo, voci):
    print('%s: %d' % (titolo, len(voci)))
    for x in voci[:20]:
        print('  ' + corto(x))
    if len(voci) > 20:
        print('  ... e altre %d' % (len(voci) - 20))


def confronta_istruzioni(a, b):
    """Confronta due liste di istruzioni che non hanno la stessa sequenza di token. True se differiscono solo gli spostamenti ammessi."""
    ca = Counter(testi for _, testi in a)
    cb = Counter(testi for _, testi in b)
    tolte = list((ca - cb).elements())
    nuove = list((cb - ca).elements())
    if tolte or nuove:
        elenca('KO istruzioni che non ci sono piu', tolte)
        elenca('KO istruzioni nuove', nuove)
        return False
    carico_a = [testi for tipo, testi in a if tipo == 'c']
    carico_b = [testi for tipo, testi in b if tipo == 'c']
    if carico_a != carico_b:
        print('KO stesse istruzioni, ma quelle eseguite al caricamento hanno cambiato ordine:')
        n = next(i for i, (x, y) in enumerate(zip(carico_a, carico_b)) if x != y)
        print('  prima: ' + corto(carico_a[n]))
        print('  dopo:  ' + corto(carico_b[n]))
        return False
    print('ok: stesse istruzioni (%d), spostate solo dichiarazioni di funzione' % len(a))
    return True


def cmd_uguali(prima, dopo):
    js_a, resto_a = leggi(prima)
    js_b, resto_b = leggi(dopo)
    ta = [t.testo for t in token_di(js_a)]
    tb = [t.testo for t in token_di(js_b)]
    ok = True
    if ta == tb:
        print('ok: stessa sequenza di codice (%d token)' % len(ta))
    else:
        ok = confronta_istruzioni(istruzioni(token_di(js_a)), istruzioni(token_di(js_b)))
    if resto_a != resto_b:
        print('KO il resto della pagina (HTML e CSS) e diverso, primo punto diverso:')
        n = next((i for i, (x, y) in enumerate(zip(resto_a, resto_b)) if x != y), min(len(resto_a), len(resto_b)))
        print('  prima: ' + resto_a[max(0, n - 40):n + 60])
        print('  dopo:  ' + resto_b[max(0, n - 40):n + 60])
        ok = False
    else:
        print('ok: resto della pagina uguale, a parita di spazi')
    return 0 if ok else 1


def cmd_doppi(pagina):
    js, _ = leggi(pagina)
    trovati = nomi_globali(token_di(js))
    doppi = sorted(n for n, voci in trovati.items() if len(voci) > 1)
    print('nomi globali: %d' % len(trovati))
    for n in doppi:
        voci = ', '.join('%s riga %d' % v for v in trovati[n])
        print('KO %s: %s (righe del codice JS, non del file)' % (n, voci))
    if doppi:
        return 1
    print('ok: nessun nome doppio')
    return 0


def main():
    a = sys.argv[1:]
    try:
        if len(a) == 3 and a[0] == 'uguali':
            return cmd_uguali(a[1], a[2])
        if a and a[0] == 'doppi' and len(a) <= 2:
            return cmd_doppi(a[1] if len(a) == 2 else PAGINA)
    except ErroreLettura as e:
        print('ERRORE: %s' % e)
        return 2
    print('uso: controllojs.py uguali <prima> <dopo>  oppure  controllojs.py doppi [<pagina>]')
    return 2


if __name__ == '__main__':
    sys.exit(main())
