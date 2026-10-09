#!/usr/bin/env python3
# VesevOS - prova delle rotte API SULLA SCHEDA VERA (da lanciare dal computer, in sola lettura + prove di errore innocue).
# Uso:  python3 apitest.py https://192.168.x.x --user admin [--probe401]
#   Chiede la password sul terminale (non si vede mentre scrivi, non viene salvata ne stampata), entra come fa la pagina
#   (prova HMAC: la password non viaggia), fa le prove e alla fine esce (logout). Se hai l'MFA chiede anche il codice.
#   Un solo tentativo di accesso: se la password e sbagliata si ferma (ogni errore conta per il blocco dell'IP).
#   In alternativa: --cookie "NOME=VALORE" (cookie di una sessione gia aperta).
#   --probe401 : prova anche 2 richieste SENZA sessione (aumentano il contatore di errori del tuo IP: pochi, ma contano per il blocco).
# Cosa controlla: (1) ogni rotta GET risponde 200 e (se JSON) il corpo e JSON valido e senza ok:false;
#   (2) /api/status e /api/common dicono apiVersion uguale a docs/API.md; (3) alcune prove di errore che NON cambiano nulla
#   (file inesistente, id sbagliato...) rispondono 4xx con ok:false e un "code" stabile; (4) rotta inesistente = 404.
# Non esegue nessuna POST che cambia qualcosa. Certificato autofirmato: la verifica TLS e spenta (solo per questa prova).
import sys, time, json, ssl, gzip, hashlib, hmac, getpass, urllib.request, urllib.error, urllib.parse, os, re
ok = bad = 0
VERSIONE = '1.7.31b'     # cambia a ogni nuova consegna del file (lettera): serve a capire quale copia stai usando
args = sys.argv[1:]
print('apitest', VERSIONE)
if not args or args[0].startswith('-'): print(__doc__); sys.exit(2)
base = args[0].rstrip('/'); cookie = ''
if '--cookie' in args: cookie = args[args.index('--cookie') + 1]
user = args[args.index('--user') + 1] if '--user' in args else ''
ctx = ssl.create_default_context(); ctx.check_hostname = False; ctx.verify_mode = ssl.CERT_NONE
HERE = os.path.dirname(os.path.abspath(__file__))
def raw(r):
    b = r.read()
    if (r.headers.get('Content-Encoding') or '') == 'gzip': b = gzip.decompress(b)
    return b.decode('utf-8', 'replace')
def call(method, path, data=None, ck=True):
    # La scheda tiene pochi collegamenti HTTPS (RAM) e ne chiude i vecchi: pausa tra le richieste e fino a 4 tentativi se chiude di colpo.
    last = None
    for tent in range(4):
        time.sleep(0.35 if tent == 0 else 1.5)
        req = urllib.request.Request(base + path, method=method, data=urllib.parse.urlencode(data).encode() if data is not None else None)
        if ck and cookie: req.add_header('Cookie', cookie)
        if data is not None: req.add_header('Content-Type', 'application/x-www-form-urlencoded')
        try:
            r = urllib.request.urlopen(req, timeout=20, context=ctx); return r.status, raw(r), r.headers
        except urllib.error.HTTPError as e:
            return e.code, raw(e), e.headers
        except (ConnectionError, OSError, ssl.SSLError) as e:
            last = e
    print('KO  connessione chiusa dalla scheda (4 tentativi):', method, path, '-', last)
    global bad
    bad += 1
    return 0, '', {}

def sha(x): return hashlib.sha256(x.encode()).hexdigest()
def pow_solve(nonce, bits):
    if not bits: return ''
    hx, rest, n = bits // 4, bits % 4, 0
    while True:
        h = sha('%s:%d' % (nonce, n))
        if h[:hx] == '0' * hx and (not rest or int(h[hx], 16) < (16 >> rest)): return str(n)
        n += 1
def do_login(u):
    global cookie
    pw = getpass.getpass('Password di %s (non si vede): ' % u)
    st, body, _ = call('POST', '/api/login/start', {'u': u}, ck=False)
    s = json.loads(body)
    if s.get('ok') is False: print('Accesso rifiutato:', s.get('err')); sys.exit(1)
    h = sha(s['salt'] + pw)
    for _ in range(int(s['iter'])): h = sha(h + s['salt'] + pw)
    mac = hmac.new(h.encode(), s['nonce'].encode(), hashlib.sha256).hexdigest()
    st, body, hd = call('POST', '/api/login', {'u': u, 'nonce': s['nonce'], 'mac': mac, 'pw': pow_solve(s['nonce'], int(s.get('pow') or 0)), 'hp': ''}, ck=False)
    j = json.loads(body)
    if j.get('mfa'):
        code = input('Codice MFA (o di recupero): ').strip()
        st, body, hd = call('POST', '/api/login/mfa', {'tok': j['tok'], 'code': code, 'now': str(int(__import__('time').time()))}, ck=False)
        j = json.loads(body)
    if not j.get('ok'): print('Accesso rifiutato:', j.get('err')); sys.exit(1)
    m = re.search(r'(vos=[^;]+)', hd.get('Set-Cookie') or '')
    if not m: print('Accesso riuscito ma nessun cookie di sessione ricevuto'); sys.exit(1)
    cookie = m.group(1); print('Accesso eseguito come', u)
if user and not cookie: do_login(user)
def chk(name, cond, why=''):
    global ok, bad
    if cond: ok += 1
    else: bad += 1; print('KO ', name, why)
SKIP = {'/', '/api/config/download', '/api/fs/get', '/api/tls/cert', '/api/stats.csv', '/api/langfile', '/api/log', '/api/tasks',
        '/api/selftest/report', '/api/fs/text', '/api/license'}
routes = [l.split() for l in open(os.path.join(HERE, 'api_routes.txt')) if l.strip()]
try: apiv = re.search(r'#define\s+VOS_API_VERSION\s+(\d+)', open(os.path.join(HERE, '..', 'VesevOS', 'src', 'core', 'vos_common.h')).read()).group(1)
except Exception: apiv = '1'      # fuori dal repo (script e api_routes.txt nella stessa cartella): versione API attuale
for m, p, lv in routes:
    if m != 'GET' or p in SKIP: continue
    if lv != 'pubblico' and not cookie: continue
    path = p + ('?path=/' if p in ('/api/fs/list',) else '') + ('?dir=/' if p == '/api/fs/dirs' else '')
    st, body, h = call('GET', path)
    chk('GET ' + p + ' -> 200', st == 200, 'HTTP %d' % st)
    if st == 200 and 'json' in (h.get('Content-Type') or ''):
        try:
            j = json.loads(body); chk('GET ' + p + ' senza ok:false', not (isinstance(j, dict) and j.get('ok') is False), body[:120])
        except Exception as e: chk('GET ' + p + ' JSON valido', False, str(e))
st, body, _ = call('GET', '/api/common', ck=False)
try: chk('apiVersion in /api/common = %s' % apiv, json.loads(body).get('apiVersion') == int(apiv), 'letto: %s' % body[:80])
except Exception as e: chk('/api/common JSON', False, str(e))
if cookie:
    st, body, _ = call('GET', '/api/status')
    try: chk('apiVersion in /api/status = %s' % apiv, json.loads(body).get('apiVersion') == int(apiv))
    except Exception as e: chk('/api/status JSON', False, str(e))
    NEG = [('POST', '/api/fs/del', {'path': '/vesevos_prova_inesistente'}), ('POST', '/api/fs/ren', {'from': '/vesevos_prova_inesistente', 'to': '/vesevos_prova_x'}),
           ('POST', '/api/users/del', {'i': '99'}), ('POST', '/api/ban/unban', {'ip': 'non-un-ip'}), ('POST', '/api/dev/act', {'id': 'inesistente', 'a': 'x'}),
           ('POST', '/api/rules/run', {'i': '999'})]
    for m, p, d in NEG:
        st, body, _ = call(m, p, d)
        try: j = json.loads(body)
        except Exception: j = {}
        chk('%s %s (errore voluto) -> 4xx, ok:false, code' % (m, p), 400 <= st < 500 and j.get('ok') is False and bool(j.get('code')), 'HTTP %d %s' % (st, body[:100]))
st, body, _ = call('GET', '/api/non-esiste-vesevos')
chk('rotta inesistente -> 404', st == 404, 'HTTP %d' % st)
if '--probe401' in args:
    for m, p in (('GET', '/api/log/level'), ('POST', '/api/reboot')):     # (reboot e POST senza sessione: viene rifiutata PRIMA di eseguire)
        st, body, _ = call(m, p, {} if m == 'POST' else None, ck=False)
        chk('%s %s senza sessione -> 401 unauthorized' % (m, p), st == 401 and '"unauthorized"' in body, 'HTTP %d' % st)
if user and cookie:
    call('POST', '/api/logout', {}); print('Uscito (logout).')
print('prove ok: %d, KO: %d' % (ok, bad)); sys.exit(1 if bad else 0)
