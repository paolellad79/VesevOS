// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_web_auth.cpp
// Percorsi del server web: accesso, MFA, utenti. Aiuti e tipi condivisi: vos_web_int.h.
#include "vos_web_int.h"
#include "../core/vos_ram.h"
#include "../net/vos_net.h"
#include "../net/vos_traffic.h"
#include "../drivers/vos_drv_time.h"

// Risposta JSON da un HeapBuf; se la memoria non e bastata, errore al posto di un JSON troncato
static esp_err_t jsonOrOom(Res* s, const HeapBuf& j) {
  if (j.oom() || j.over()) return ko(s, tr("Memoria insufficiente"), "off");
  return sendJson(s, j);
}

// {"ok":true,"rec":"codici"} (i codici di recupero sono solo lettere maiuscole, cifre, trattino e spazio: niente da proteggere)
static esp_err_t sendRecovery(Res* s, const char* rec) {
  HeapBuf j(160);
  j.addf("{\"ok\":true,\"rec\":\"%s\"}", rec);
  return jsonOrOom(s, j);
}

void routesAuth(PsychicHttpServer* S, bool sec) {
  // ---- accesso: 1) sale + numero casuale  2) prova (HMAC). La password non viaggia mai. ----
  add(S, sec, "/api/login/start", HTTP_POST, L_PUB, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    uint32_t w = 0;
    if (authIpBlocked(c.ip, w)) return ko(s, trf("Troppi errori da questo indirizzo: riprova tra %lu secondi", (unsigned long)w).c_str(), "blocked");
    HeapBuf j(256);
    authLoginStart(Pc(r, "u"), j);
    return sendJson(s, j);
  });
  add(S, sec, "/api/login", HTTP_POST, L_PUB, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    int idx = -1; uint32_t w = 0;
    if (*Pc(r, "hp")) { authFailIp(c.ip, "campo trappola compilato"); return ko(s, tr("Nome o password errati"), "bad_login"); }      // un browser vero non lo compila
    if (!powCheck(Pc(r, "nonce"), Pc(r, "pw"))) return ko(s, tr("Prova di lavoro non valida: ricarica la pagina e riprova"));
    int res = authLoginFinish(c.ip, Pc(r, "u"), Pc(r, "nonce"), Pc(r, "mac"), idx, w);
    if (res == 2) return ko(s, trf("Troppi errori da questo indirizzo: riprova tra %lu secondi", (unsigned long)w).c_str(), "blocked");
    if (res != 0) return ko(s, tr("Nome o password errati"), "bad_login");
    if (mfaOn(idx)) {                                   // password giusta: manca il codice
      char tk[MFA_TOK_BUF];
      if (!mfaLoginToken(idx, tk)) return ko(s, tr("Ora della scheda non valida: accesso con MFA bloccato. Usa il tasto BOOT 8 secondi o imposta l'ora dalla seriale"));
      HeapBuf j(256);
      j.addf("{\"ok\":false,\"mfa\":true,\"tok\":\"%s\",\"notime\":%s,\"recOnly\":%s}", tk, mfaLoginNoTime(tk) ? "true" : "false",
             !timeValid() && cfg.mfaNoTime == 1 ? "true" : "false");
      return sendJson(s, j);
    }
    return loginDone(s, c, idx);
  });
  add(S, sec, "/api/login/mfa", HTTP_POST, L_PUB, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    uint32_t w = 0;
    if (authIpBlocked(c.ip, w)) return ko(s, trf("Troppi errori da questo indirizzo: riprova tra %lu secondi", (unsigned long)w).c_str(), "blocked");
    int idx = -1; ErrBuf e;
    int res = mfaLoginCheck(Pc(r, "tok"), Pc(r, "code"), (uint32_t)Pint(r, "now"), idx, e);
    if (res == 0) return loginDone(s, c, idx);
    if (res == 3) return ko(s, tr("Ora della scheda non valida: il browser non ha mandato la sua ora"));
    if (res == 2) { authFailIp(c.ip, "gettone MFA scaduto o tentativi finiti"); return ko(s, tr("Tempo scaduto o troppi errori: rifai l'accesso dall'inizio"), "bad_login"); }
    authFailIp(c.ip, "codice MFA errato");
    return ko(s, tr("Codice errato"), "bad_login");
  });
  // ---- MFA: gestione (ognuno il proprio; l'amministratore puo spegnere quello degli altri) ----
  add(S, sec, "/api/mfa", HTTP_GET, L_GUEST, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    HeapBuf j(MFA_JSON_MAX);
    mfaJson(c.role == ROLE_ADMIN ? -1 : c.user, j);
    return jsonOrOom(s, j);
  });
  add(S, sec, "/api/mfa/begin", HTTP_POST, L_GUEST, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    if (!c.secure && !netOnAp(c.ip)) return ko(s, tr("Per sicurezza il QR si mostra solo con HTTPS o dall'hotspot"), "forbidden");
    HeapBuf j(MFA_JSON_MAX);
    return mfaBegin(c.user, cfg.hostname.c_str(), j) ? sendJson(s, j) : ko(s, tr("Errore"));
  });
  add(S, sec, "/api/mfa/confirm", HTTP_POST, L_GUEST, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    char rec[MFA_REC_BUF]; ErrBuf e;
    if (!mfaConfirm(c.user, Pc(r, "code"), rec, e)) return ko(s, e);
    cfgSetOriginf("MFA %s", cfg.users[c.user].name.c_str()); cfgSave();
    return sendRecovery(s, rec);
  });
  add(S, sec, "/api/mfa/recovery", HTTP_POST, L_GUEST, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    if (!c.secure && !netOnAp(c.ip)) return ko(s, tr("Per sicurezza i codici si mostrano solo con HTTPS o dall'hotspot"), "forbidden");
    char rec[MFA_REC_BUF]; if (!mfaNewRecovery(c.user, rec)) return ko(s, tr("MFA non attivo"), "state");
    cfgSetOriginf("MFA codici %s", cfg.users[c.user].name.c_str()); cfgSave();
    return sendRecovery(s, rec);
  });
  add(S, sec, "/api/mfa/off", HTTP_POST, L_GUEST, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    int i = has(r, "i") ? (int)Pint(r, "i") : c.user;
    if (i != c.user && c.role != ROLE_ADMIN) return ko(s, tr("Solo l'amministratore puo spegnere l'MFA di un altro utente"), "forbidden");
    mfaOff(i); cfgSetOriginf("MFA spento %d", i); cfgSave(); return ok(s);
  });
  add(S, sec, "/api/mfa/set", HTTP_POST, L_ADMIN, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    if (has(r, "pow")) { int v = (int)Pint(r, "pow"); if (!(v == 0 || (v >= 8 && v <= 20))) return ko(s, tr("Difficolta non valida")); cfg.powBits = v; }
    if (has(r, "nt")) { int v = (int)Pint(r, "nt"); if (v < 0 || v > 2) return ko(s, tr("Valore non valido")); cfg.mfaNoTime = v; }
    cfgSetOrigin("MFA impostazioni"); cfgSave();
    HeapBuf j(MFA_JSON_MAX);
    mfaJson(-1, j);
    return jsonOrOom(s, j);
  });
  // primo accesso: si sceglie la password dell'amministratore (solo se non ce n'e nessuno)
  add(S, sec, "/api/firstpass", HTTP_POST, L_PUB, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    if (authIsSet()) return ko(s, tr("La password e gia impostata"), "state");
    bool fromAp = netOnAp(c.ip), fresh = !cfg.staEnabled || (g_firstPassUntil && (int32_t)(g_firstPassUntil - drvMillis()) > 0);
    if (!fromAp && !fresh) return ko(s, tr("Per sicurezza la prima password si sceglie dall'hotspot della scheda, oppure entro 10 minuti dal reset con il tasto BOOT"), "forbidden");
    int a = authFirstAdmin();
    ErrBuf e;
    if (a < 0 || !userSetPassword(a, Pc(r, "p"), e)) return ko(s, e.empty() ? tr("Errore") : e.c_str());
    cfg.users[a].on = true; cfg.users[a].role = ROLE_ADMIN;
    cfgSetOriginf("primo accesso %s", IpText(c.ip).s); cfgSave();
    char t[AUTH_TOKEN_BUF];
    authNewSession(t, a, c.ip, c.secure);
    char ck[160];
    snprintf(ck, sizeof ck, "vos=%s" COOKIE_ATTRS "%s", t, c.secure ? "; Secure" : "");
    s->addHeader("Set-Cookie", ck);
    HeapBuf j(256);
    j.add("{\"ok\":true,\"user\":\""); j.addJson(cfg.users[a].name.c_str(), cfg.users[a].name.length()); j.add("\",\"role\":2}");
    return jsonOrOom(s, j);
  });
  add(S, sec, "/api/logout", HTTP_POST, L_PUB, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    { char tk[AUTH_TOKEN_BUF]; tokenTo(r, tk, sizeof tk); authLogout(tk); } ramNote("logout");
    s->addHeader("Set-Cookie", "vos=; Path=/; Max-Age=0");
    return ok(s);
  });

  // ---- utenti ----
  add(S, sec, "/api/users", HTTP_GET, L_ADMIN, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    HeapBuf j(AUTH_TEXT_MAX);
    usersJson(j);
    return jsonOrOom(s, j);
  });
  add(S, sec, "/api/users/add", HTTP_POST, L_ADMIN, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    ErrBuf e;
    if (!userAdd(Pc(r, "name"), (uint8_t)Pint(r, "role"), Pc(r, "pass"), e)) return ko(s, e);
    cfgSave(); return ok(s);
  });
  add(S, sec, "/api/users/set", HTTP_POST, L_ADMIN, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    ErrBuf e;
    int role = has(r, "role") ? (int)Pint(r, "role") : -1, on = has(r, "on") ? (int)Pint(r, "on") : -1;
    if (!userSet((int)Pint(r, "i"), role, on, e)) return ko(s, e);
    cfgSave(); return ok(s);
  });
  add(S, sec, "/api/users/del", HTTP_POST, L_ADMIN, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    ErrBuf e;
    if (!userDel((int)Pint(r, "i"), e)) return ko(s, e);
    cfgSave(); return ok(s);
  });
  add(S, sec, "/api/users/pass", HTTP_POST, L_ADMIN, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    ErrBuf e;
    if (!userSetPassword((int)Pint(r, "i"), Pc(r, "pass"), e)) return ko(s, e);
    cfgSave(); return ok(s);
  });
  add(S, sec, "/api/passwd", HTTP_POST, L_GUEST, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    if (!userCheckPassword(c.user, Pc(r, "o"))) return ko(s, tr("Vecchia password errata"), "bad_login");
    ErrBuf e;
    if (!userSetPassword(c.user, Pc(r, "n"), e)) return ko(s, e);
    cfgSave(); return ok(s);
  });

  add(S, sec, "/api/ban", HTTP_GET, L_ADMIN, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    HeapBuf j(AUTH_TEXT_MAX);
    authBanJson(j);
    return jsonOrOom(s, j);
  });
  add(S, sec, "/api/ban/unban", HTTP_POST, L_ADMIN, [](Req* r, Res* s, Ctx& c) -> esp_err_t { return authUnban(Pc(r, "ip")) ? ok(s) : ko(s, tr("Indirizzo non trovato"), "notfound"); });
  add(S, sec, "/api/ban/set", HTTP_POST, L_ADMIN, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    int f = (int)Pint(r, "fails"); long sc = Pint(r, "secs");
    if (f < 3 || f > 20 || sc < 10 || sc > 3600) return ko(s, tr("Valori non validi"));
    cfg.banFails = f; cfg.banSecs = sc; cfgSave(); return ok(s);
  });

  add(S, sec, "/api/status", HTTP_GET, L_GUEST, [](Req* r, Res* s, Ctx& c) -> esp_err_t { 
    HeapBuf j(SYS_STATUS_JSON_MAX + 2048);          // net e traffico stanno sopra il nucleo: si aggiungono qui
    if (!sysStatusJson(j)) return ko(s, tr("Memoria insufficiente"), "off");
    char traf[TRAFFIC_JSON_MAX];
    trafficJson(traf, sizeof(traf));
    j.cutLast();                                    // toglie la } finale per aggiungere net e traffico
    j.add(",\"net\":"); netStatusJson(j); j.add(','); j.add(traf); j.add('}');
    if (j.oom() || j.over()) return ko(s, tr("Memoria insufficiente"), "off");
    return sendText(s, 200, "application/json", j.c_str(), j.length()); });
  add(S, sec, "/api/settings", HTTP_GET, L_GUEST, [](Req* r, Res* s, Ctx& c) -> esp_err_t { 
    HeapBuf j(SETTINGS_JSON_MAX);
    if (!settingsJson(j)) return ko(s, tr("Memoria insufficiente"), "off");
    return sendJson(s, j); });
}
