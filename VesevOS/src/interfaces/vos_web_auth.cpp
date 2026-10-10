// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_web_auth.cpp
// Percorsi del server web: accesso, MFA, utenti. Aiuti e tipi condivisi: vos_web_int.h.
#include "vos_web_int.h"
#include "../core/vos_ram.h"

void routesAuth(PsychicHttpServer* S, bool sec) {
  // ---- accesso: 1) sale + numero casuale  2) prova (HMAC). La password non viaggia mai. ----
  add(S, sec, "/api/login/start", HTTP_POST, L_PUB, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    uint32_t w = 0;
    if (authIpBlocked(c.ip, w)) return ko(s, trf("Troppi errori da questo indirizzo: riprova tra %lu secondi", (unsigned long)w), "blocked");
    return sendJson(s, authLoginStart(P(r, "u")));
  });
  add(S, sec, "/api/login", HTTP_POST, L_PUB, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    int idx = -1; uint32_t w = 0;
    if (P(r, "hp").length()) { authFailIp(c.ip, "campo trappola compilato"); return ko(s, tr("Nome o password errati"), "bad_login"); }      // un browser vero non lo compila
    if (!powCheck(P(r, "nonce"), P(r, "pw"))) return ko(s, tr("Prova di lavoro non valida: ricarica la pagina e riprova"));
    int res = authLoginFinish(c.ip, P(r, "u"), P(r, "nonce"), P(r, "mac"), idx, w);
    if (res == 2) return ko(s, trf("Troppi errori da questo indirizzo: riprova tra %lu secondi", (unsigned long)w), "blocked");
    if (res != 0) return ko(s, tr("Nome o password errati"), "bad_login");
    if (mfaOn(idx)) {                                   // password giusta: manca il codice
      bool blk = false; String tk = mfaLoginToken(idx, blk);
      if (blk) return ko(s, tr("Ora della scheda non valida: accesso con MFA bloccato. Usa il tasto BOOT 8 secondi o imposta l'ora dalla seriale"));
      return sendJson(s, "{\"ok\":false,\"mfa\":true,\"tok\":\"" + tk + "\",\"notime\":" + String(mfaLoginNoTime(tk) ? "true" : "false") + ",\"recOnly\":" + String(!timeValid() && cfg.mfaNoTime == 1 ? "true" : "false") + "}");
    }
    return loginDone(s, c, idx);
  });
  add(S, sec, "/api/login/mfa", HTTP_POST, L_PUB, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    uint32_t w = 0;
    if (authIpBlocked(c.ip, w)) return ko(s, trf("Troppi errori da questo indirizzo: riprova tra %lu secondi", (unsigned long)w), "blocked");
    int idx = -1; String e;
    int res = mfaLoginCheck(P(r, "tok"), P(r, "code"), (uint32_t)P(r, "now").toInt(), idx, e);
    if (res == 0) return loginDone(s, c, idx);
    if (res == 3) return ko(s, tr("Ora della scheda non valida: il browser non ha mandato la sua ora"));
    if (res == 2) { authFailIp(c.ip, "gettone MFA scaduto o tentativi finiti"); return ko(s, tr("Tempo scaduto o troppi errori: rifai l'accesso dall'inizio"), "bad_login"); }
    authFailIp(c.ip, "codice MFA errato");
    return ko(s, tr("Codice errato"), "bad_login");
  });
  // ---- MFA: gestione (ognuno il proprio; l'amministratore puo spegnere quello degli altri) ----
  add(S, sec, "/api/mfa", HTTP_GET, L_GUEST, [](Req* r, Res* s, Ctx& c) -> esp_err_t { return sendJson(s, mfaJson(c.role == ROLE_ADMIN ? -1 : c.user)); });
  add(S, sec, "/api/mfa/begin", HTTP_POST, L_GUEST, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    if (!c.secure && !netOnAp(c.ip)) return ko(s, tr("Per sicurezza il QR si mostra solo con HTTPS o dall'hotspot"), "forbidden");
    String j = mfaBegin(c.user, cfg.hostname);
    return j.length() ? sendJson(s, j) : ko(s, tr("Errore"));
  });
  add(S, sec, "/api/mfa/confirm", HTTP_POST, L_GUEST, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    String rec, e;
    if (!mfaConfirm(c.user, P(r, "code"), rec, e)) return ko(s, e);
    cfgSetOrigin("MFA " + cfg.users[c.user].name); cfgSave();
    return sendJson(s, "{\"ok\":true,\"rec\":\"" + rec + "\"}");
  });
  add(S, sec, "/api/mfa/recovery", HTTP_POST, L_GUEST, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    if (!c.secure && !netOnAp(c.ip)) return ko(s, tr("Per sicurezza i codici si mostrano solo con HTTPS o dall'hotspot"), "forbidden");
    String rec; if (!mfaNewRecovery(c.user, rec)) return ko(s, tr("MFA non attivo"), "state");
    cfgSetOrigin("MFA codici " + cfg.users[c.user].name); cfgSave();
    return sendJson(s, "{\"ok\":true,\"rec\":\"" + rec + "\"}");
  });
  add(S, sec, "/api/mfa/off", HTTP_POST, L_GUEST, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    int i = has(r, "i") ? (int)P(r, "i").toInt() : c.user;
    if (i != c.user && c.role != ROLE_ADMIN) return ko(s, tr("Solo l'amministratore puo spegnere l'MFA di un altro utente"), "forbidden");
    mfaOff(i); cfgSetOrigin("MFA spento " + String(i)); cfgSave(); return ok(s);
  });
  add(S, sec, "/api/mfa/set", HTTP_POST, L_ADMIN, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    if (has(r, "pow")) { int v = P(r, "pow").toInt(); if (!(v == 0 || (v >= 8 && v <= 20))) return ko(s, tr("Difficolta non valida")); cfg.powBits = v; }
    if (has(r, "nt")) { int v = P(r, "nt").toInt(); if (v < 0 || v > 2) return ko(s, tr("Valore non valido")); cfg.mfaNoTime = v; }
    cfgSetOrigin("MFA impostazioni"); cfgSave(); return sendJson(s, mfaJson(-1));
  });
  // primo accesso: si sceglie la password dell'amministratore (solo se non ce n'e nessuno)
  add(S, sec, "/api/firstpass", HTTP_POST, L_PUB, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    if (authIsSet()) return ko(s, tr("La password e gia impostata"), "state");
    bool fromAp = netOnAp(c.ip), fresh = !cfg.staEnabled || (g_firstPassUntil && (int32_t)(g_firstPassUntil - millis()) > 0);
    if (!fromAp && !fresh) return ko(s, tr("Per sicurezza la prima password si sceglie dall'hotspot della scheda, oppure entro 10 minuti dal reset con il tasto BOOT"), "forbidden");
    int a = authFirstAdmin();
    String e;
    if (a < 0 || !userSetPassword(a, P(r, "p"), e)) return ko(s, e.length() ? e : String(tr("Errore")));
    cfg.users[a].on = true; cfg.users[a].role = ROLE_ADMIN;
    cfgSetOrigin("primo accesso " + ipToStr(c.ip)); cfgSave();
    String t = authNewSession(a);
    s->addHeader("Set-Cookie", ("vos=" + t + cookieAttrs(c.secure)).c_str());
    return sendJson(s, "{\"ok\":true,\"user\":\"" + jsonEscape(cfg.users[a].name) + "\",\"role\":2}");
  });
  add(S, sec, "/api/logout", HTTP_POST, L_PUB, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    authLogout(token(r)); ramNote("logout");
    s->addHeader("Set-Cookie", "vos=; Path=/; Max-Age=0");
    return ok(s);
  });

  // ---- utenti ----
  add(S, sec, "/api/users", HTTP_GET, L_ADMIN, [](Req* r, Res* s, Ctx& c) -> esp_err_t { return sendJson(s, usersJson()); });
  add(S, sec, "/api/users/add", HTTP_POST, L_ADMIN, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    String e;
    if (!userAdd(P(r, "name"), P(r, "role").toInt(), P(r, "pass"), e)) return ko(s, e);
    cfgSave(); return ok(s);
  });
  add(S, sec, "/api/users/set", HTTP_POST, L_ADMIN, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    String e;
    int role = has(r, "role") ? (int)P(r, "role").toInt() : -1, on = has(r, "on") ? (int)P(r, "on").toInt() : -1;
    if (!userSet(P(r, "i").toInt(), role, on, e)) return ko(s, e);
    cfgSave(); return ok(s);
  });
  add(S, sec, "/api/users/del", HTTP_POST, L_ADMIN, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    String e;
    if (!userDel(P(r, "i").toInt(), e)) return ko(s, e);
    cfgSave(); return ok(s);
  });
  add(S, sec, "/api/users/pass", HTTP_POST, L_ADMIN, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    String e;
    if (!userSetPassword(P(r, "i").toInt(), P(r, "pass"), e)) return ko(s, e);
    cfgSave(); return ok(s);
  });
  add(S, sec, "/api/passwd", HTTP_POST, L_GUEST, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    if (!userCheckPassword(c.user, P(r, "o"))) return ko(s, tr("Vecchia password errata"), "bad_login");
    String e;
    if (!userSetPassword(c.user, P(r, "n"), e)) return ko(s, e);
    cfgSave(); return ok(s);
  });

  add(S, sec, "/api/ban", HTTP_GET, L_ADMIN, [](Req* r, Res* s, Ctx& c) -> esp_err_t { return sendJson(s, authBanJson()); });
  add(S, sec, "/api/ban/unban", HTTP_POST, L_ADMIN, [](Req* r, Res* s, Ctx& c) -> esp_err_t { return authUnban(P(r, "ip")) ? ok(s) : ko(s, tr("Indirizzo non trovato"), "notfound"); });
  add(S, sec, "/api/ban/set", HTTP_POST, L_ADMIN, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    int f = P(r, "fails").toInt(); long sc = P(r, "secs").toInt();
    if (f < 3 || f > 20 || sc < 10 || sc > 3600) return ko(s, tr("Valori non validi"));
    cfg.banFails = f; cfg.banSecs = sc; cfgSave(); return ok(s);
  });

  add(S, sec, "/api/status", HTTP_GET, L_GUEST, [](Req* r, Res* s, Ctx& c) -> esp_err_t { return sendJson(s, sysStatusJson()); });
  add(S, sec, "/api/settings", HTTP_GET, L_GUEST, [](Req* r, Res* s, Ctx& c) -> esp_err_t { return sendJson(s, settingsJson()); });
}
