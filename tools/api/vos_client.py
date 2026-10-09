#!/usr/bin/env python3
# Esempio: accesso a VesevOS da Python e prime chiamate. Solo libreria standard.
# Uso:  python3 vos_client.py https://vesevos.local admin LaTuaPassword [comando-shell]
import hashlib, hmac, json, ssl, sys, urllib.parse, urllib.request, http.cookiejar

def sha(s): return hashlib.sha256(s.encode()).hexdigest()

def hash_pass(salt, pw, it):
    h = sha(salt + pw)
    for _ in range(it): h = sha(h + salt + pw)
    return h

def solve_pow(nonce, bits):
    if not bits: return ""
    n = 0
    while True:
        h = bin(int(sha(f"{nonce}:{n}"), 16))[2:].zfill(256)
        if h.startswith("0" * bits): return str(n)
        n += 1

class Vos:
    def __init__(self, base, verify=False):
        self.base = base.rstrip("/")
        ctx = ssl.create_default_context()
        if not verify:                       # certificato autofirmato: accettalo solo in rete di casa
            ctx.check_hostname = False; ctx.verify_mode = ssl.CERT_NONE
        self.op = urllib.request.build_opener(urllib.request.HTTPCookieProcessor(http.cookiejar.CookieJar()),
                                              urllib.request.HTTPSHandler(context=ctx))
    def call(self, path, data=None):
        body = urllib.parse.urlencode(data).encode() if data is not None else None
        try:
            r = self.op.open(urllib.request.Request(self.base + path, body))
            return json.loads(r.read().decode())
        except urllib.error.HTTPError as e:
            try: return json.loads(e.read().decode())
            except Exception: return {"ok": False, "err": f"HTTP {e.code}"}
    def login(self, user, pw, mfa_code=None):
        s = self.call("/api/login/start", {"u": user})
        mac = hmac.new(hash_pass(s["salt"], pw, s["iter"]).encode(), s["nonce"].encode(), hashlib.sha256).hexdigest()
        j = self.call("/api/login", {"u": user, "nonce": s["nonce"], "mac": mac,
                                     "pw": solve_pow(s["nonce"], s.get("pow", 0)), "hp": ""})
        if j.get("mfa"):
            if not mfa_code: raise SystemExit("serve il codice MFA")
            import time
            j = self.call("/api/login/mfa", {"tok": j["tok"], "code": mfa_code, "now": int(time.time())})
        return j

if __name__ == "__main__":
    if len(sys.argv) < 4: raise SystemExit(__doc__ or "uso: vos_client.py URL utente password [comando]")
    v = Vos(sys.argv[1]); r = v.login(sys.argv[2], sys.argv[3])
    print("login:", r.get("ok"), r.get("err", ""))
    if r.get("ok"):
        print(json.dumps(v.call("/api/status"), indent=1)[:600])
        if len(sys.argv) > 4: print(v.call("/api/shell", {"c": " ".join(sys.argv[4:])}))
