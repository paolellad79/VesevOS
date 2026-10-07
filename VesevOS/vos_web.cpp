// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_web.cpp
// Server web con PsychicHttp (MIT), basato sul server ufficiale di ESP-IDF.
//  - porta 443 (HTTPS): pagina e API cifrate, certificato unico della scheda (vos_tls)
//  - porta 80  (HTTP):  portale automatico e, per chi e collegato all'hotspot (gia cifrato da WPA2), anche la pagina;
//                       dalla rete di casa porta a https://
// Ogni API ha un livello: pubblica, Ospite, Operatore, Amministratore (controllo nel firmware, non solo nella pagina).
// Il filtro IP (vos_fw) chiude la connessione senza risposta a chi non e ammesso.
#include "vos_web.h"
#include "vos_i18n.h"
#include "vos_license.h"
#include "vos_page.h"
#include "vos_config.h"
#include "vos_auth.h"
#include "vos_sys.h"
#include "vos_net.h"
#include "vos_mqtt.h"
#include "vos_led.h"
#include "vos_pins.h"
#include "vos_shell.h"
#include "vos_util.h"
#include "vos_files.h"
#include "vos_time.h"
#include "vos_rules.h"
#include "vos_boot.h"
#include "vos_region.h"
#include "vos_audit.h"
#include "vos_fw.h"
#include "vos_wd.h"
#include "vos_tls.h"
#include "vos_mesh.h"
#include "vos_ble.h"
#include "vos_mfa.h"
#include "vos_power.h"
#include "vos_stats.h"
#include "vos_log.h"
#include "vos_selftest.h"
#include "vos_diario.h"
#include <LittleFS.h>
#include <WiFi.h>
#include <PsychicHttp.h>
#include <PsychicHttpsServer.h>

typedef PsychicRequest Req;
typedef PsychicResponse Res;
enum Lvl : uint8_t { L_PUB = 0, L_GUEST, L_OPER, L_ADMIN };
struct Ctx { int user; uint8_t role; uint32_t ip; bool secure; };
typedef esp_err_t (*Handler)(Req*, Res*, Ctx&);

static PsychicHttpServer* g_http = nullptr;
static PsychicHttpsServer* g_https = nullptr;
static bool g_tlsUp = false;
static volatile bool g_cleanArm = false;   // fine guida con Wi-Fi di casa: al primo accesso dalla rete di casa si spengono hotspot e HTTP (solo in RAM)
static uint16_t g_runHttpPort = 0, g_runHttpsPort = 0;   // porte con cui i server sono partiti (0 = non attivo)
static uint32_t g_firstPassUntil = 0;     // dopo il reset con BOOT: 10 minuti per scegliere la password anche dalla rete di casa
static File g_upFile;
static bool g_upOk = false;
static String g_upErr, g_upPath;

// Stampa su String (per la shell web)
class StrPrint : public Print {
 public:
  String s;
  size_t write(uint8_t c) override { if (s.length() < 6000) s += (char)c; return 1; }
  size_t write(const uint8_t* b, size_t n) override { for (size_t i = 0; i < n; i++) write(b[i]); return n; }
};

static uint32_t ipOf(Req* r) {
  IPAddress a = r->client()->remoteIP();
  return ((uint32_t)a[0] << 24) | ((uint32_t)a[1] << 16) | ((uint32_t)a[2] << 8) | (uint32_t)a[3];
}

static String P(Req* r, const char* name) {
  PsychicWebParameter* p = r->getParam(name);
  return p ? p->value() : String("");
}
static bool has(Req* r, const char* name) { return r->hasParam(name); }

// parametro dalla riga dell'indirizzo (?nome=valore), anche nelle richieste di caricamento
static String qparam(Req* r, const char* name) {
  String q = r->query(), k = String(name) + "=";
  int p = 0;
  while (p < (int)q.length()) {
    int e = q.indexOf('&', p); if (e < 0) e = q.length();
    String kv = q.substring(p, e);
    if (kv.startsWith(k)) return urlDecode(kv.substring(k.length()).c_str());
    p = e + 1;
  }
  return "";
}

static esp_err_t sendText(Res* s, int code, const char* type, const String& body) {
  s->setCode(code);
  s->setContentType(type);
  s->addHeader("Cache-Control", "no-store");
  s->setContent((const uint8_t*)body.c_str(), body.length());
  return s->send();
}
static esp_err_t sendJson(Res* s, const String& body, int code = 200) { return sendText(s, code, "application/json", body); }
static esp_err_t ok(Res* s) { return sendJson(s, "{\"ok\":true}"); }
static esp_err_t ko(Res* s, const String& e) { return sendJson(s, "{\"ok\":false,\"err\":\"" + jsonEscape(e) + "\"}"); }
static esp_err_t notFound(Res* s) { return sendText(s, 404, "text/plain; charset=utf-8", tr("Non trovato")); }

static String token(Req* r) { return r->hasHeader("Cookie") ? authCookieFromHeader(r->header("Cookie")) : String(""); }

static String who(const Ctx& c) {
  String n = c.user >= 0 ? cfg.users[c.user].name : String("-");
  return "web " + n + " " + ipToStr(c.ip);
}

// Registra un percorso con livello di accesso. secure = server HTTPS.
static volatile uint32_t g_lastReq = 0;                // ultima richiesta dalla pagina (per non dormire mentre la usi)
uint32_t webIdleSec() { return (millis() - g_lastReq) / 1000; }

static void add(PsychicHttpServer* srv, bool secure, const char* path, int method, uint8_t lvl, Handler h) {
  srv->on(path, method, [h, lvl, secure, method](Req* r, Res* s) -> esp_err_t {
    Ctx c; c.ip = ipOf(r); c.user = -1; c.role = 0; c.secure = secure;
    g_lastReq = millis();
    if (!fwAllow(c.ip, false)) { fwNoteRejected(c.ip); return ESP_FAIL; }     // chiude senza rispondere
    // dalla rete di casa la pagina e le API passano solo da HTTPS (l'hotspot e gia cifrato da WPA2)
    if (!secure && g_tlsUp && cfg.https && !netOnAp(c.ip)) {
      if (strcmp(r->pathCStr(), "/") == 0) { String host = r->host(); int col = host.indexOf(':'); if (col > 0) host = host.substring(0, col); return s->redirect(("https://" + host + (cfg.httpsPort != 443 ? ":" + String(cfg.httpsPort) : String("")) + "/").c_str()); }
      if (lvl != L_PUB || method == HTTP_POST) return sendJson(s, "{\"ok\":false,\"err\":\"https\"}", 403);   // anche accesso e prima password
    }
    if (lvl > L_PUB) {
      int u = authSessionUser(token(r));
      if (u < 0) { authNoteDenied(c.ip); return sendJson(s, "{\"ok\":false,\"err\":\"non autorizzato\"}", 401); }
      c.user = u; c.role = cfg.users[u].role;
      if (c.role + 1 < lvl) return sendJson(s, "{\"ok\":false,\"err\":\"" + jsonEscape(tr("Il tuo ruolo non permette questa operazione")) + "\"}", 403);
    }
    if (method == HTTP_POST) cfgSetOrigin(who(c));
    return h(r, s, c);
  });
}

static String svcJson() {
  bool pend = (cfg.httpOn != (g_runHttpPort != 0)) || (cfg.httpOn && g_runHttpPort && cfg.httpPort != g_runHttpPort) ||
              (cfg.https != g_tlsUp) || (cfg.https && g_runHttpsPort && cfg.httpsPort != g_runHttpsPort);
  return String("{\"apOn\":") + (cfg.apOn ? "true" : "false") + ",\"captive\":" + (cfg.apCaptive ? "true" : "false") +
         ",\"dhcpOn\":" + (cfg.dhcpOn ? "true" : "false") + ",\"lease\":" + String(cfg.dhcpLease) + ",\"mdnsOn\":" + (cfg.mdnsOn ? "true" : "false") +
         ",\"httpOn\":" + (cfg.httpOn ? "true" : "false") + ",\"httpPort\":" + String(cfg.httpPort) +
         ",\"httpsOn\":" + (cfg.https ? "true" : "false") + ",\"httpsPort\":" + String(cfg.httpsPort) +
         ",\"runHttp\":" + String(g_runHttpPort) + ",\"runHttps\":" + String(g_runHttpsPort) +
         ",\"pending\":" + (pend ? "true" : "false") + ",\"sta\":" + (cfg.staEnabled && cfg.staSsid.length() ? "true" : "false") +
         ",\"clientOk\":" + (netState() == NET_CLIENT_OK ? "true" : "false") + "}";
}

static String settingsJson() {
  String j = "{";
  j += "\"hostname\":\"" + jsonEscape(cfg.hostname) + "\",\"domain\":\"" + jsonEscape(cfg.domain) + "\",";
  j += "\"serialAuth\":" + String(cfg.serialAuth ? "true" : "false") + ",";
  j += "\"staSsid\":\"" + jsonEscape(cfg.staSsid) + "\",";
  j += "\"staDhcp\":" + String(cfg.staDhcp ? "true" : "false") + ",";
  j += "\"ip\":\"" + jsonEscape(cfg.ip) + "\",\"mask\":\"" + jsonEscape(cfg.mask) + "\",";
  j += "\"gw\":\"" + jsonEscape(cfg.gw) + "\",\"dns1\":\"" + jsonEscape(cfg.dns1) + "\",\"dns2\":\"" + jsonEscape(cfg.dns2) + "\",";
  j += "\"apSsid\":\"" + jsonEscape(cfg.apSsid) + "\",";
  j += "\"ledMode\":" + String(cfg.ledMode) + ",\"ledColor\":" + String(cfg.ledColor) + ",\"ledBrightness\":" + String(cfg.ledBrightness);
  j += "}";
  return j;
}

static void delayedRestart(void*) { vTaskDelay(pdMS_TO_TICKS(800)); ESP.restart(); }
static volatile uint32_t g_sleepSec = 0;
static void delayedSleep(void*) { vTaskDelay(pdMS_TO_TICKS(800)); if (g_sleepSec) powerSleepNow(g_sleepSec); else sysSleep(); }

static esp_err_t sendPage(Res* s) {
  s->addHeader("Content-Encoding", "gzip");
  s->addHeader("Cache-Control", "no-store");
  return s->send(200, "text/html; charset=utf-8", INDEX_GZ, INDEX_GZ_LEN);
}

static String cookieAttrs(bool secure) { return String("; Path=/; HttpOnly; SameSite=Strict") + (secure ? "; Secure" : ""); }

// dopo password (e codice MFA) giusti: sessione, cookie, pulizia di fine guida
static esp_err_t loginDone(Res* s, Ctx& c, int idx) {
  String t = authNewSession(idx);
  vlog("WEB: accesso di %s da %s", cfg.users[idx].name.c_str(), ipToStr(c.ip).c_str());
  bool cleaned = false;
  if (g_cleanArm && cfg.users[idx].role == ROLE_ADMIN && !netOnAp(c.ip) && netState() == NET_CLIENT_OK && cfg.staEnabled) {
    // la scheda e stata raggiunta dalla rete di casa: si spengono le cose servite solo all'hotspot
    g_cleanArm = false;
    cfg.apOn = false; cfg.apCaptive = false;
    if (cfg.https && g_tlsUp) cfg.httpOn = false;       // mai chiusi fuori: HTTP si spegne solo se HTTPS funziona
    cfgSetOrigin("fine guida " + ipToStr(c.ip)); cfgSave(); cleaned = true;
    vlog("SETUP: raggiunta dalla rete di casa, spenti hotspot e HTTP");
  }
  if (cfg.users[idx].mfaOn) cfgSave();                  // il contatore MFA non deve tornare indietro
  s->addHeader("Set-Cookie", ("vos=" + t + cookieAttrs(c.secure)).c_str());
  return sendJson(s, "{\"ok\":true,\"user\":\"" + jsonEscape(cfg.users[idx].name) + "\",\"role\":" + String(cfg.users[idx].role) + (cleaned ? ",\"clean\":true" : "") + "}");
}

// ---------------- tutti i percorsi (registrati su entrambi i server) ----------------
static void routes(PsychicHttpServer* S, bool sec) {
  add(S, sec, "/", HTTP_GET, L_PUB, [](Req* r, Res* s, Ctx& c) -> esp_err_t { return sendPage(s); });

  // dati comuni (paesi, mappa, regole radio, elenco librerie): pubblici, uguali per tutti, conservati dal browser
  add(S, sec, "/api/common", HTTP_GET, L_PUB, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    size_t n; const uint8_t* gz = regionCommonGz(n);
    s->addHeader("Content-Encoding", "gzip");
    s->addHeader("Cache-Control", "public, max-age=86400");
    return s->send(200, "application/json", gz, n);
  });

  add(S, sec, "/api/auth", HTTP_GET, L_PUB, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    int u = authSessionUser(token(r));
    String j = String("{\"set\":") + (authIsSet() ? "true" : "false") + ",\"setup\":" + (cfg.setupDone ? "true" : "false") +
               ",\"lang\":\"" + jsonEscape(cfg.lang) + "\",\"secure\":" + (c.secure ? "true" : "false") +
               ",\"https\":" + (g_tlsUp && cfg.https ? "true" : "false") + ",\"fp\":\"" + (g_tlsUp ? tlsFingerprint() : String("")) + "\"" +
               ",\"ble\":" + (VOS_WITH_BLE ? "true" : "false") + ",\"hp\":" + String(g_runHttpPort) + ",\"sp\":" + String(g_runHttpsPort) + timePubJson();
    if (!authIsSet()) { int a = authFirstAdmin(); j += ",\"first\":\"" + jsonEscape(a >= 0 ? cfg.users[a].name : String("admin")) + "\""; }
    if (u >= 0) j += ",\"user\":\"" + jsonEscape(cfg.users[u].name) + "\",\"role\":" + String(cfg.users[u].role);
    return sendJson(s, j + "}");
  });

  // Licenze e note legali (pubbliche, leggibili anche prima dell'accesso)
  add(S, sec, "/api/license", HTTP_GET, L_PUB, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    if (!has(r, "id")) return sendJson(s, licListJson());
    int li = licFind(P(r, "id"));
    if (li < 0) return notFound(s);
    if (licZ(li)) { s->addHeader("Content-Encoding", "gzip"); return s->send(200, "text/plain; charset=utf-8", (const uint8_t*)licText(li), licZ(li)); }   // licenze lunghe: compresse, il browser le apre
    return s->send(200, "text/plain; charset=utf-8", (const uint8_t*)licText(li), licSize(li));
  });

  // Lingue (pubbliche: servono anche alla schermata di accesso, contengono solo testi)
  add(S, sec, "/api/langs", HTTP_GET, L_PUB, [](Req* r, Res* s, Ctx& c) -> esp_err_t { return sendJson(s, langListJson()); });
  add(S, sec, "/api/langfile", HTTP_GET, L_PUB, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    String code = P(r, "code");
    size_t n = 0; const uint8_t* b = langBuiltinJson(code, n);
    if (b) { s->addHeader("Cache-Control", "no-cache"); s->addHeader("Content-Encoding", "gzip"); return s->send(200, "application/json; charset=utf-8", b, n); }
    String path = "/lang/" + code + ".json";
    if (code == "it" || !langCodeValid(code) || !LittleFS.exists(path)) return notFound(s);
    PsychicFileResponse f(s, LittleFS, path, "application/json; charset=utf-8");
    return f.send();
  });
  add(S, sec, "/api/lang", HTTP_POST, L_ADMIN, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    String code = P(r, "code"); code.trim(); code.toLowerCase();
    String e;
    return langSet(code, e) ? ok(s) : ko(s, e);
  });

  // ---- accesso: 1) sale + numero casuale  2) prova (HMAC). La password non viaggia mai. ----
  add(S, sec, "/api/login/start", HTTP_POST, L_PUB, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    uint32_t w = 0;
    if (authIpBlocked(c.ip, w)) return ko(s, trf("Troppi errori da questo indirizzo: riprova tra %lu secondi", (unsigned long)w));
    return sendJson(s, authLoginStart(P(r, "u")));
  });
  add(S, sec, "/api/login", HTTP_POST, L_PUB, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    int idx = -1; uint32_t w = 0;
    if (P(r, "hp").length()) { authFailIp(c.ip, "campo trappola compilato"); return ko(s, tr("Nome o password errati")); }      // un browser vero non lo compila
    if (!powCheck(P(r, "nonce"), P(r, "pw"))) return ko(s, tr("Prova di lavoro non valida: ricarica la pagina e riprova"));
    int res = authLoginFinish(c.ip, P(r, "u"), P(r, "nonce"), P(r, "mac"), idx, w);
    if (res == 2) return ko(s, trf("Troppi errori da questo indirizzo: riprova tra %lu secondi", (unsigned long)w));
    if (res != 0) return ko(s, tr("Nome o password errati"));
    if (mfaOn(idx)) {                                   // password giusta: manca il codice
      bool blk = false; String tk = mfaLoginToken(idx, blk);
      if (blk) return ko(s, tr("Ora della scheda non valida: accesso con MFA bloccato. Usa il tasto BOOT 8 secondi o imposta l'ora dalla seriale"));
      return sendJson(s, "{\"ok\":false,\"mfa\":true,\"tok\":\"" + tk + "\",\"notime\":" + String(mfaLoginNoTime(tk) ? "true" : "false") + ",\"recOnly\":" + String(!timeValid() && cfg.mfaNoTime == 1 ? "true" : "false") + "}");
    }
    return loginDone(s, c, idx);
  });
  add(S, sec, "/api/login/mfa", HTTP_POST, L_PUB, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    uint32_t w = 0;
    if (authIpBlocked(c.ip, w)) return ko(s, trf("Troppi errori da questo indirizzo: riprova tra %lu secondi", (unsigned long)w));
    int idx = -1; String e;
    int res = mfaLoginCheck(P(r, "tok"), P(r, "code"), (uint32_t)P(r, "now").toInt(), idx, e);
    if (res == 0) return loginDone(s, c, idx);
    if (res == 3) return ko(s, tr("Ora della scheda non valida: il browser non ha mandato la sua ora"));
    if (res == 2) { authFailIp(c.ip, "gettone MFA scaduto o tentativi finiti"); return ko(s, tr("Tempo scaduto o troppi errori: rifai l'accesso dall'inizio")); }
    authFailIp(c.ip, "codice MFA errato");
    return ko(s, tr("Codice errato"));
  });
  // ---- MFA: gestione (ognuno il proprio; l'amministratore puo spegnere quello degli altri) ----
  add(S, sec, "/api/mfa", HTTP_GET, L_GUEST, [](Req* r, Res* s, Ctx& c) -> esp_err_t { return sendJson(s, mfaJson(c.role == ROLE_ADMIN ? -1 : c.user)); });
  add(S, sec, "/api/mfa/begin", HTTP_POST, L_GUEST, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    if (!c.secure && !netOnAp(c.ip)) return ko(s, tr("Per sicurezza il QR si mostra solo con HTTPS o dall'hotspot"));
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
    if (!c.secure && !netOnAp(c.ip)) return ko(s, tr("Per sicurezza i codici si mostrano solo con HTTPS o dall'hotspot"));
    String rec; if (!mfaNewRecovery(c.user, rec)) return ko(s, tr("MFA non attivo"));
    cfgSetOrigin("MFA codici " + cfg.users[c.user].name); cfgSave();
    return sendJson(s, "{\"ok\":true,\"rec\":\"" + rec + "\"}");
  });
  add(S, sec, "/api/mfa/off", HTTP_POST, L_GUEST, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    int i = has(r, "i") ? (int)P(r, "i").toInt() : c.user;
    if (i != c.user && c.role != ROLE_ADMIN) return ko(s, tr("Solo l'amministratore puo spegnere l'MFA di un altro utente"));
    mfaOff(i); cfgSetOrigin("MFA spento " + String(i)); cfgSave(); return ok(s);
  });
  add(S, sec, "/api/mfa/set", HTTP_POST, L_ADMIN, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    if (has(r, "pow")) { int v = P(r, "pow").toInt(); if (!(v == 0 || (v >= 8 && v <= 20))) return ko(s, tr("Difficolta non valida")); cfg.powBits = v; }
    if (has(r, "nt")) { int v = P(r, "nt").toInt(); if (v < 0 || v > 2) return ko(s, tr("Valore non valido")); cfg.mfaNoTime = v; }
    cfgSetOrigin("MFA impostazioni"); cfgSave(); return sendJson(s, mfaJson(-1));
  });
  // primo accesso: si sceglie la password dell'amministratore (solo se non ce n'e nessuno)
  add(S, sec, "/api/firstpass", HTTP_POST, L_PUB, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    if (authIsSet()) return ko(s, tr("La password e gia impostata"));
    bool fromAp = netOnAp(c.ip), fresh = !cfg.staEnabled || (g_firstPassUntil && (int32_t)(g_firstPassUntil - millis()) > 0);
    if (!fromAp && !fresh) return ko(s, tr("Per sicurezza la prima password si sceglie dall'hotspot della scheda, oppure entro 10 minuti dal reset con il tasto BOOT"));
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
    authLogout(token(r));
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
    if (!userCheckPassword(c.user, P(r, "o"))) return ko(s, tr("Vecchia password errata"));
    String e;
    if (!userSetPassword(c.user, P(r, "n"), e)) return ko(s, e);
    cfgSave(); return ok(s);
  });

  add(S, sec, "/api/ban", HTTP_GET, L_ADMIN, [](Req* r, Res* s, Ctx& c) -> esp_err_t { return sendJson(s, authBanJson()); });
  add(S, sec, "/api/ban/unban", HTTP_POST, L_ADMIN, [](Req* r, Res* s, Ctx& c) -> esp_err_t { authUnban(P(r, "ip")); return ok(s); });
  add(S, sec, "/api/ban/set", HTTP_POST, L_ADMIN, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    int f = P(r, "fails").toInt(); long sc = P(r, "secs").toInt();
    if (f < 3 || f > 20 || sc < 10 || sc > 3600) return ko(s, tr("Valori non validi"));
    cfg.banFails = f; cfg.banSecs = sc; cfgSave(); return ok(s);
  });

  add(S, sec, "/api/status", HTTP_GET, L_GUEST, [](Req* r, Res* s, Ctx& c) -> esp_err_t { return sendJson(s, sysStatusJson()); });
  add(S, sec, "/api/settings", HTTP_GET, L_GUEST, [](Req* r, Res* s, Ctx& c) -> esp_err_t { return sendJson(s, settingsJson()); });
  add(S, sec, "/api/pins", HTTP_GET, L_GUEST, [](Req* r, Res* s, Ctx& c) -> esp_err_t { return sendJson(s, pinsJson()); });
  add(S, sec, "/api/pins/info", HTTP_GET, L_GUEST, [](Req* r, Res* s, Ctx& c) -> esp_err_t { return sendJson(s, pinBoardJson()); });
  add(S, sec, "/api/pinmap", HTTP_GET, L_GUEST, [](Req* r, Res* s, Ctx& c) -> esp_err_t { return sendJson(s, pinMapJson()); });
  add(S, sec, "/api/pinnotes", HTTP_GET, L_GUEST, [](Req* r, Res* s, Ctx& c) -> esp_err_t { return sendJson(s, pinNotesJson()); });
  add(S, sec, "/api/pinnotes", HTTP_POST, L_ADMIN, [](Req* r, Res* s, Ctx& c) -> esp_err_t { String e; return pinNoteSet(P(r, "g").toInt(), P(r, "name"), e) ? ok(s) : ko(s, e); });
  add(S, sec, "/api/pintest", HTTP_GET, L_GUEST, [](Req* r, Res* s, Ctx& c) -> esp_err_t { return sendJson(s, pinTestJson()); });
  add(S, sec, "/api/pintest", HTTP_POST, L_OPER, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    String err;
    return pinTestRequest(P(r, "gpio").toInt(), P(r, "action"), P(r, "pull"), err) ? ok(s) : ko(s, err);
  });
  add(S, sec, "/api/log", HTTP_GET, L_GUEST, [](Req* r, Res* s, Ctx& c) -> esp_err_t { return sendText(s, 200, "text/plain; charset=utf-8", logGet(150)); });
  add(S, sec, "/api/log/clear", HTTP_POST, L_ADMIN, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    logClear(); auditEvent(AUD_INFO, "", trf("registro svuotato (%s)", who(c).c_str())); return ok(s);
  });
  add(S, sec, "/api/log/level", HTTP_GET, L_GUEST, [](Req* r, Res* s, Ctx& c) -> esp_err_t { return sendJson(s, "{\"lv\":" + String((int)logLevel()) + "}"); });
  add(S, sec, "/api/log/level", HTTP_POST, L_ADMIN, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    int lv = constrain((int)P(r, "lv").toInt(), 0, 3);
    cfg.logLevel = (uint8_t)lv; logSetLevel(lv); cfgSave();
    auditEvent(AUD_INFO, "", trf("livello del registro %d (%s)", lv, who(c).c_str()));
    return ok(s);
  });
  add(S, sec, "/api/boots", HTTP_GET, L_OPER, [](Req* r, Res* s, Ctx& c) -> esp_err_t { return sendText(s, 200, "text/plain; charset=utf-8", diaryText(20, true)); });
  add(S, sec, "/api/boots/clear", HTTP_POST, L_ADMIN, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    diaryClear(); auditEvent(AUD_INFO, "", trf("diario dei riavvii azzerato (%s)", who(c).c_str())); return ok(s);
  });
  add(S, sec, "/api/tasks", HTTP_GET, L_OPER, [](Req* r, Res* s, Ctx& c) -> esp_err_t { return sendText(s, 200, "text/plain; charset=utf-8", sysTasksText()); });

  add(S, sec, "/api/serialauth", HTTP_POST, L_ADMIN, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    cfg.serialAuth = (P(r, "on") != "0"); cfgSave();
    vlog("SICUREZZA: password sulla seriale %s", cfg.serialAuth ? "attivata" : "disattivata");
    return ok(s);
  });
  add(S, sec, "/api/identify", HTTP_POST, L_OPER, [](Req* r, Res* s, Ctx& c) -> esp_err_t { ledIdentify(10000); return ok(s); });
  add(S, sec, "/api/tasklist", HTTP_GET, L_OPER, [](Req* r, Res* s, Ctx& c) -> esp_err_t { return sendJson(s, sysTasksJson()); });
  add(S, sec, "/api/taskkill", HTTP_POST, L_ADMIN, [](Req* r, Res* s, Ctx& c) -> esp_err_t { String err; return sysTaskKill(P(r, "name"), err) ? ok(s) : ko(s, err); });
  add(S, sec, "/api/taskrestart", HTTP_POST, L_ADMIN, [](Req* r, Res* s, Ctx& c) -> esp_err_t { String err; return sysTaskRestart(P(r, "name"), err) ? ok(s) : ko(s, err); });

  // Automazioni
  add(S, sec, "/api/rules", HTTP_GET, L_OPER, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    return sendJson(s, "{\"text\":\"" + jsonEscape(rulesText()) + "\",\"st\":" + rulesStatusJson() + "}");
  });
  add(S, sec, "/api/rules", HTTP_POST, L_OPER, [](Req* r, Res* s, Ctx& c) -> esp_err_t { String e; return rulesSave(P(r, "text"), e) ? ok(s) : ko(s, e); });
  add(S, sec, "/api/rules/run", HTTP_POST, L_OPER, [](Req* r, Res* s, Ctx& c) -> esp_err_t { String e; return rulesRunNow(P(r, "i").toInt(), e) ? ok(s) : ko(s, e); });
  // Ordine di avvio
  add(S, sec, "/api/boot", HTTP_GET, L_OPER, [](Req* r, Res* s, Ctx& c) -> esp_err_t { return sendJson(s, bootJson()); });
  add(S, sec, "/api/boot", HTTP_POST, L_ADMIN, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    String res, e;
    if (!bootSetOrder(P(r, "order"), res, e)) return ko(s, e);
    return sendJson(s, "{\"ok\":true,\"order\":\"" + jsonEscape(res) + "\"}");
  });
  add(S, sec, "/api/boot/reset", HTTP_POST, L_ADMIN, [](Req* r, Res* s, Ctx& c) -> esp_err_t { bootResetOrder(); return ok(s); });
  add(S, sec, "/api/wifi/scan", HTTP_GET, L_OPER, [](Req* r, Res* s, Ctx& c) -> esp_err_t { return sendJson(s, netScanJson()); });
  add(S, sec, "/api/wifi/scan", HTTP_POST, L_OPER, [](Req* r, Res* s, Ctx& c) -> esp_err_t { netScanStart(); return ok(s); });

  add(S, sec, "/api/wifi/test", HTTP_GET, L_ADMIN, [](Req* r, Res* s, Ctx& c) -> esp_err_t { return sendJson(s, netTestJson()); });
  add(S, sec, "/api/wifi/test", HTTP_POST, L_ADMIN, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    String ssid = P(r, "ssid"), pass = P(r, "pass");
    if (ssid.length() == 0 || ssid.length() > 32) return ko(s, tr("Nome rete non valido"));
    if (pass.length() > 0 && (pass.length() < 8 || pass.length() > 63)) return ko(s, tr("Password Wi-Fi: da 8 a 63 caratteri"));
    netTestStart(ssid, pass); return ok(s);
  });
  add(S, sec, "/api/wifi/save", HTTP_POST, L_ADMIN, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    String ssid = P(r, "ssid"), pass = P(r, "pass");
    bool dhcp = P(r, "dhcp") != "0";
    if (ssid.length() == 0 || ssid.length() > 32) return ko(s, tr("Nome rete non valido"));
    if (pass.length() > 0 && (pass.length() < 8 || pass.length() > 63)) return ko(s, tr("Password Wi-Fi: da 8 a 63 caratteri"));
    if (!dhcp) {
      uint32_t a, m, g, d;
      if (!ipParse(P(r, "ip"), a)) return ko(s, tr("IP non valido"));
      if (!ipParse(P(r, "mask"), m) || !maskValid(m)) return ko(s, tr("Subnet mask non valida"));
      if (!ipParse(P(r, "gw"), g)) return ko(s, tr("Gateway non valido"));
      if (((a ^ g) & m) != 0) return ko(s, tr("Il gateway non e nella stessa rete dell'IP"));
      if (P(r, "d1").length() && !ipParse(P(r, "d1"), d)) return ko(s, tr("DNS 1 non valido"));
      if (P(r, "d2").length() && !ipParse(P(r, "d2"), d)) return ko(s, tr("DNS 2 non valido"));
      cfg.ip = P(r, "ip"); cfg.mask = P(r, "mask"); cfg.gw = P(r, "gw");
      cfg.dns1 = P(r, "d1"); cfg.dns2 = P(r, "d2");
    }
    g_cleanArm = P(r, "cleanup") == "1";
    cfg.staSsid = ssid;
    if (pass.length()) cfg.staPass = pass;
    cfg.staDhcp = dhcp; cfg.staEnabled = true;
    cfgSave(); netReconfigure(); return ok(s);
  });
  add(S, sec, "/api/wifi/ap", HTTP_POST, L_ADMIN, [](Req* r, Res* s, Ctx& c) -> esp_err_t { cfg.staEnabled = false; cfgSave(); netReconfigure(); return ok(s); });

  // Punto di accesso (hotspot): nome e password. Dopo l'accesso li vede chiunque (scelta del proprietario:
  // il QR stampato da fiducia a chi lo usa); l'amministratore puo nasconderli agli altri (interruttore "apQr").
  add(S, sec, "/api/ap", HTTP_GET, L_GUEST, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    bool show = cfg.apQr || c.role >= L_ADMIN;
    return sendJson(s, "{\"ssid\":\"" + jsonEscape(cfg.apSsid) + "\",\"pass\":\"" + (show ? jsonEscape(cfg.apPass) : String("")) + "\",\"qr\":" + (cfg.apQr ? "true" : "false") + "}");
  });
  add(S, sec, "/api/ap", HTTP_POST, L_ADMIN, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    if (has(r, "qr") && !has(r, "ssid")) { cfg.apQr = (P(r, "qr") == "1"); cfgSave(); return ok(s); }
    String ss = P(r, "ssid"), pw = P(r, "pass");
    ss.trim();
    if (!ss.length() || ss.length() > 32) return ko(s, tr("Nome rete non valido"));
    if (P(r, "new") == "1") pw = cfgNewApPass();
    if (pw.length() && (pw.length() < 8 || pw.length() > 63)) return ko(s, tr("Password Wi-Fi: da 8 a 63 caratteri"));
    if (pw.length() && cfgApPassWeak(pw)) return ko(s, tr("Password troppo debole o di fabbrica: le leggi sulla sicurezza (UE RED/EN 18031, CRA; UK PSTI) vietano le password uguali per tutti"));
    cfg.apSsid = ss; if (pw.length()) cfg.apPass = pw;
    cfgSave(); netReconfigure();
    return sendJson(s, "{\"ok\":true,\"pass\":\"" + jsonEscape(cfg.apPass) + "\"}");
  });

  add(S, sec, "/api/shell", HTTP_POST, L_OPER, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    String cmd = P(r, "c");
    if (cmd.length() > 200) return sendText(s, 200, "text/plain; charset=utf-8", tr("Comando troppo lungo"));
    StrPrint sp;
    cfgSetOrigin("shell " + who(c));
    shellExec(cmd, sp, c.role);
    return sendText(s, 200, "text/plain; charset=utf-8", cleanAscii(sp.s));
  });

  add(S, sec, "/api/led", HTTP_POST, L_OPER, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    int m = P(r, "mode").toInt(), b = P(r, "br").toInt();
    if (m < 0 || m > 3 || b < 0 || b > 255) return ko(s, tr("Valori non validi"));
    cfg.ledMode = m; cfg.ledBrightness = b;
    cfg.ledColor = (uint32_t)strtoul(P(r, "color").c_str(), NULL, 10) & 0xFFFFFF;
    ledApplyConfig(); cfgSave(); return ok(s);
  });

  add(S, sec, "/api/system", HTTP_POST, L_ADMIN, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    String hn = P(r, "hostname"), dm = P(r, "domain");
    hn.trim(); dm.trim(); hn.toLowerCase(); dm.toLowerCase();
    if (!hostnameValid(hn)) return ko(s, tr("Nome host non valido (1-32 caratteri: lettere, numeri, trattino; non all'inizio o alla fine)"));
    if (!domainValid(dm)) return ko(s, tr("Dominio non valido (nomi separati da punti, solo lettere, numeri e trattino)"));
    bool changed = (hn != cfg.hostname);
    cfg.hostname = hn; cfg.domain = dm;
    cfgSave();
    if (changed) netReconfigure();
    return ok(s);
  });

  add(S, sec, "/api/config/download", HTTP_GET, L_ADMIN, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    s->addHeader("Content-Disposition", "attachment; filename=\"vesevos.conf\"");
    return sendText(s, 200, "text/plain; charset=utf-8", cfgExport(false));
  });
  add(S, sec, "/api/config/restore", HTTP_POST, L_ADMIN, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    String t = P(r, "t"), err;
    if (t.length() == 0 || t.length() > 12000) return ko(s, tr("File vuoto o troppo grande"));
    if (!cfgImport(t, err)) return ko(s, err);
    cfgSave(); ledApplyConfig(); netReconfigure(); return ok(s);
  });

  add(S, sec, "/api/airplane", HTTP_POST, L_OPER, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    if (P(r, "on") != "1") { netAirplaneOff("dalla pagina"); return ok(s); }
    String err;
    return netAirplaneOn(P(r, "exit").toInt(), strtoul(P(r, "param").c_str(), NULL, 10), err) ? ok(s) : ko(s, err);
  });

  // ---- MQTT (servizio spento di fabbrica: la prima volta si avvia a mano) ----
  add(S, sec, "/api/home", HTTP_GET, L_GUEST, [](Req* r, Res* s, Ctx& c) -> esp_err_t {      // un solo pezzo per i widget della Home
    String j = "{\"mesh\":" + String(meshRunning() ? 1 : 0) + ",\"role\":" + String((int)cfg.meshRole) + ",\"nodes\":" + String(meshNodeCount()) +
               ",\"ntp\":" + String(cfg.ntpOn ? 1 : 0) + ",\"last\":" + String((unsigned long)timeLastSync()) +
               ",\"ble\":" + String(bleRunning() ? 1 : 0) + ",\"bleLeft\":" + String((unsigned long)bleLeftSec()) +
               ",\"pw\":" + String((int)cfg.pwMode) + ",\"stat\":" + String(cfg.statOn ? 1 : 0) + timePubJson() + "}";
    return sendJson(s, j);
  });
  add(S, sec, "/api/mqtt", HTTP_GET, L_OPER, [](Req* r, Res* s, Ctx& c) -> esp_err_t { return sendJson(s, mqttStatusJson()); });
  add(S, sec, "/api/mqtt", HTTP_POST, L_ADMIN, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    String host = P(r, "host"), pre = P(r, "prefix"), user = P(r, "user");
    long port = P(r, "port").toInt(), ev = P(r, "every").toInt();
    host.trim(); pre.trim(); user.trim();
    if (host.length() > 80 || user.length() > 60 || pre.length() > 60) return ko(s, tr("Valori troppo lunghi"));
    for (size_t i = 0; i < host.length(); i++) { char ch = host[i]; if (!(isAlphaNumeric(ch) || ch == '.' || ch == '-')) return ko(s, tr("Broker: solo lettere, numeri, punto e trattino")); }
    for (size_t i = 0; i < pre.length(); i++) { char ch = pre[i]; if (ch == '#' || ch == '+' || ch < 33 || ch > 126) return ko(s, tr("Prefisso: niente spazi, # o +")); }
    if (port < 1 || port > 65535 || ev < 5 || ev > 3600) return ko(s, tr("Valori non validi"));
    cfg.mqttHost = host; cfg.mqttPort = port; cfg.mqttUser = user; cfg.mqttPrefix = pre; cfg.mqttEvery = ev;
    if (has(r, "pass") && P(r, "pass").length()) cfg.mqttPass = P(r, "pass");
    if (P(r, "clearpass") == "1") cfg.mqttPass = "";
    cfg.mqttAuto = P(r, "auto") == "1"; cfg.mqttHa = P(r, "ha") == "1"; cfg.mqttTls = P(r, "tls") == "1";
    cfgSave();
    if (mqttRunning()) mqttStart();      // riparte con i valori nuovi
    return ok(s);
  });
  add(S, sec, "/api/mqtt/ca", HTTP_POST, L_ADMIN, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    String pem = P(r, "pem"), e;
    if (!pem.length()) { LittleFS.remove("/mqtt-ca.pem"); return ok(s); }
    if (pem.indexOf("-----BEGIN CERTIFICATE-----") < 0 || pem.length() > 8000) return ko(s, tr("Certificato non valido (formato PEM)"));
    return fsWriteText("/mqtt-ca.pem", pem, e) ? ok(s) : ko(s, e);
  });
  add(S, sec, "/api/mqtt/run", HTTP_POST, L_OPER, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    String a = P(r, "a");
    if (a == "start" || a == "restart") { if (!cfg.mqttHost.length()) return ko(s, tr("Manca l'indirizzo del broker")); mqttStart(); }
    else if (a == "stop") mqttStop();
    else if (a == "test") { if (!mqttPublishRel("test", "VesevOS " VOS_VERSION)) return ko(s, tr("MQTT non collegato")); }
    else return ko(s, tr("Azione non valida"));
    return ok(s);
  });
  add(S, sec, "/api/reboot", HTTP_POST, L_OPER, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    vlog("SISTEMA: riavvio dalla pagina (%s)", who(c).c_str());
    esp_err_t e = ok(s);
    xTaskCreate(delayedRestart, "rst", 2048, NULL, 1, NULL);
    return e;
  });
  add(S, sec, "/api/sleep", HTTP_POST, L_OPER, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    g_sleepSec = has(r, "min") ? (uint32_t)(P(r, "min").toInt() * 60L) : 0;     // 0 = fino a RESET
    if (g_sleepSec > 7UL * 24 * 3600) return ko(s, tr("Sonno: da 1 minuto a 7 giorni"));
    esp_err_t e = ok(s);
    xTaskCreate(delayedSleep, "slp", 3072, NULL, 1, NULL);
    return e;
  });
  // ---- autodiagnosi: prove una per volta + report di testo (solo Admin) ----
  add(S, sec, "/api/selftest", HTTP_GET, L_ADMIN, [](Req* r, Res* s, Ctx& c) -> esp_err_t { return sendJson(s, selftestJson()); });
  add(S, sec, "/api/selftest", HTTP_POST, L_ADMIN, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    String e;
    if (P(r, "clear") == "1") { selftestClear(); return ok(s); }
    if (!selftestStart(P(r, "active") == "1", P(r, "names") == "1", e)) return ko(s, e);
    return ok(s);
  });
  add(S, sec, "/api/selftest/report", HTTP_GET, L_ADMIN, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    String rp = selftestReport();
    if (!rp.length()) return ko(s, tr("Nessun report: lancia prima la prova"));
    s->addHeader("Content-Disposition", "attachment; filename=\"vesevos-report.txt\"");
    return sendText(s, 200, "text/plain; charset=utf-8", rp);
  });
  add(S, sec, "/api/stats", HTTP_GET, L_OPER, [](Req* r, Res* s, Ctx& c) -> esp_err_t { return sendJson(s, statsJson()); });
  add(S, sec, "/api/stats.csv", HTTP_GET, L_OPER, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    s->addHeader("Content-Disposition", "attachment; filename=\"vesevos-statistiche.csv\"");
    return sendText(s, 200, "text/csv", statsCsv());
  });
  add(S, sec, "/api/stats", HTTP_POST, L_ADMIN, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    if (has(r, "on")) { bool on = P(r, "on") == "1"; if (on && !cfg.statOn) statsReset(); cfg.statOn = on; cfgSetOrigin("statistiche"); cfgSave(); }
    if (P(r, "reset") == "1") statsReset();
    return sendJson(s, statsJson());
  });
  add(S, sec, "/api/power", HTTP_GET, L_OPER, [](Req* r, Res* s, Ctx& c) -> esp_err_t { return sendJson(s, powerJson()); });
  add(S, sec, "/api/power", HTTP_POST, L_ADMIN, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    String e;
    powerSet((int)P(r, "mode").toInt(), (uint32_t)P(r, "awake").toInt(), (uint32_t)P(r, "sleep").toInt(), e);
    if (e.length()) return ko(s, e);
    cfgSetOrigin("risparmio energia"); cfgSave();
    return sendJson(s, powerJson());
  });
  add(S, sec, "/api/factory", HTTP_POST, L_ADMIN, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    vlog("SISTEMA: ripristino di fabbrica dalla pagina (%s)", who(c).c_str());
    cfgFactoryReset();
    esp_err_t e = ok(s);
    xTaskCreate(delayedRestart, "rst", 2048, NULL, 1, NULL);
    return e;
  });

  // ---- ora / NTP ----
  add(S, sec, "/api/time", HTTP_GET, L_GUEST, [](Req* r, Res* s, Ctx& c) -> esp_err_t { return sendJson(s, timeJson()); });
  add(S, sec, "/api/time", HTTP_POST, L_ADMIN, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    String srv = P(r, "server");
    if (srv.length() < 1 || srv.length() > 60) return ko(s, tr("Server NTP non valido"));
    for (size_t i = 0; i < srv.length(); i++) { char ch = srv[i]; if (!(isAlphaNumeric(ch) || ch == '.' || ch == '-')) return ko(s, tr("Server NTP: solo lettere, numeri, punto e trattino")); }
    long ev = has(r, "every") ? P(r, "every").toInt() : (long)cfg.ntpEvery;
    if (!timeEveryValid(ev)) return ko(s, tr("Frequenza non valida"));
    cfg.ntpEvery = ev;
    cfg.ntpOn = P(r, "ntp") == "1"; cfg.ntpServe = P(r, "serve") == "1";
    cfg.ntpServer = srv;
    cfgSave(); timeApply(); return ok(s);
  });
  add(S, sec, "/api/time/sync", HTTP_POST, L_OPER, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    if (!cfg.ntpOn) return ko(s, tr("La sincronizzazione NTP e spenta"));
    if (netState() != NET_CLIENT_OK) return ko(s, tr("Serve la connessione a una rete Wi-Fi"));
    timeApply(); vlog("TIME: sincronizzazione richiesta dalla pagina"); return ok(s);
  });
  add(S, sec, "/api/time/set", HTTP_POST, L_ADMIN, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    if (has(r, "local")) { String err; return timeSetLocal(P(r, "local"), err) ? ok(s) : ko(s, err); }
    unsigned long e = strtoul(P(r, "epoch").c_str(), NULL, 10);
    if (e < 1700000000UL) return ko(s, tr("Ora non valida"));
    timeSetEpoch(e); return ok(s);
  });
  add(S, sec, "/api/cpu", HTTP_POST, L_ADMIN, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    int m = P(r, "mode").toInt();
    if (m != 0 && m != 80 && m != 160 && m != 240) return ko(s, tr("Valore non valido"));
    cfg.cpuMhz = m; cfgSave(); sysApplyCpuMode(); return ok(s);
  });

  // ---- localizzazione: paese, fuso, formati, unita, radio ----
  add(S, sec, "/api/region", HTTP_GET, L_GUEST, [](Req* r, Res* s, Ctx& c) -> esp_err_t { return sendJson(s, regionJson()); });
  add(S, sec, "/api/region", HTTP_POST, L_ADMIN, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    bool radio = false;
    if (has(r, "country")) {
      String cc = P(r, "country"); cc.toUpperCase();
      if (cc.length() && !regionValid(cc)) return ko(s, tr("Paese sconosciuto"));
      if (cc != cfg.country) { cfg.country = cc; radio = true; }
    }
    if (has(r, "tz")) {
      String tz = P(r, "tz"), tzn = P(r, "tzname");
      if (tz.length() < 3 || tz.length() > 60) return ko(s, tr("Fuso orario non valido"));
      for (size_t i = 0; i < tz.length(); i++) { unsigned char ch = tz[i]; if (ch < 32 || ch >= 127 || ch == '\'' || ch == '"') return ko(s, tr("Fuso orario: caratteri non validi")); }
      cfg.tz = tz; cfg.tzName = tzn.length() ? cleanAscii(tzn) : String("Personalizzato");
      setenv("TZ", cfg.tz.c_str(), 1); tzset();
    }
    if (has(r, "ntp")) { String srv = P(r, "ntp"); srv.trim(); if (srv.length() >= 1 && srv.length() <= 60) cfg.ntpServer = srv; }
    if (has(r, "datefmt")) cfg.dateFmt = constrain((int)P(r, "datefmt").toInt(), 0, 2);
    if (has(r, "timefmt")) cfg.timeFmt = constrain((int)P(r, "timefmt").toInt(), 0, 1);
    if (has(r, "tempunit")) cfg.tempUnit = constrain((int)P(r, "tempunit").toInt(), 0, 1);
    if (has(r, "weekstart")) cfg.weekStart = P(r, "weekstart") == "1" ? 1 : 0;
    if (has(r, "decsep")) cfg.decSep = P(r, "decsep") == "1" ? 1 : 0;
    if (has(r, "antenna")) { uint8_t a = P(r, "antenna") == "1" ? 1 : 0; if (a != cfg.antExt) radio = true; cfg.antExt = a; }
    if (has(r, "gain")) { int g = P(r, "gain").toInt(); if (g < 0 || g > 15) return ko(s, tr("Guadagno dell'antenna: da 0 a 15 dBi")); if (g != cfg.antGain) radio = true; cfg.antGain = g; }
    if (has(r, "txpower")) { int t = P(r, "txpower").toInt(); if (t < 0 || t > 20) return ko(s, tr("Potenza non valida")); if (t != cfg.txDbm) radio = true; cfg.txDbm = t; }
    cfgSave(); timeApply();
    if (radio && !cfg.airOn) regionApplyRadio();
    return sendJson(s, regionJson());
  });

  // ---- prima configurazione ----
  add(S, sec, "/api/setup/done", HTTP_POST, L_ADMIN, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    if (has(r, "ntp")) { cfg.ntpOn = P(r, "ntp") == "1"; timeApply(); }     // senza Wi-Fi di casa: server dell'ora spento
    cfg.setupDone = true; cfgSave(); ledSetSetup(false);
    vlog("SETUP: prima configurazione completata");
    return ok(s);
  });

  // ---- controllo della configurazione (allarmi) e registro delle modifiche ----
  add(S, sec, "/api/audit", HTTP_GET, L_GUEST, [](Req* r, Res* s, Ctx& c) -> esp_err_t { return sendJson(s, auditJson()); });
  add(S, sec, "/api/audit/ack", HTTP_POST, L_OPER, [](Req* r, Res* s, Ctx& c) -> esp_err_t { auditAck(P(r, "n").toInt()); return ok(s); });

  // ---- filtro IP ----
  add(S, sec, "/api/fw", HTTP_GET, L_ADMIN, [](Req* r, Res* s, Ctx& c) -> esp_err_t { return sendJson(s, fwJson(c.ip)); });
  add(S, sec, "/api/fw", HTTP_POST, L_ADMIN, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    int mode = P(r, "mode").toInt();
    if (mode < 0 || mode > 3) return ko(s, tr("Modo non valido"));
    // voci: una per riga "indirizzo|1|nome" (indirizzo, intervallo a-b o rete/nn)
    FwRule nr[VOS_FW_MAX]; int n = 0;
    String t = P(r, "rules"); int p = 0, ln = 0;
    while (p < (int)t.length()) {
      int e = t.indexOf('\n', p); if (e < 0) e = t.length();
      String l = t.substring(p, e); l.trim(); p = e + 1; ln++;
      if (!l.length()) continue;
      if (n >= VOS_FW_MAX) return ko(s, trf("Massimo %d voci", VOS_FW_MAX));
      int a1 = l.indexOf('|'), a2 = a1 >= 0 ? l.indexOf('|', a1 + 1) : -1;
      String rng = a1 >= 0 ? l.substring(0, a1) : l;
      if (!cfgParseRange(rng, nr[n].a, nr[n].b)) return ko(s, trf("Voce %d: indirizzo non valido", ln));
      nr[n].on = a1 < 0 || l.substring(a1 + 1, a2 > a1 ? a2 : l.length()) != "0";
      nr[n].name = a2 > a1 ? cleanAscii(l.substring(a2 + 1)) : String("");
      if (nr[n].name.length() > 24) nr[n].name = nr[n].name.substring(0, 24);
      n++;
    }
    fwTryStart(c.ip);                                   // la regola nuova vale 2 minuti se non confermata
    cfg.fwMode = mode; cfg.fwNtp = P(r, "ntp") == "1"; cfg.fwN = n;
    for (int i = 0; i < n; i++) cfg.fw[i] = nr[i];
    cfgSave();
    return sendJson(s, fwJson(c.ip));
  });
  add(S, sec, "/api/fw/confirm", HTTP_POST, L_ADMIN, [](Req* r, Res* s, Ctx& c) -> esp_err_t { fwConfirm(); return ok(s); });

  // ---- watchdog ----
  add(S, sec, "/api/wd", HTTP_GET, L_OPER, [](Req* r, Res* s, Ctx& c) -> esp_err_t { return sendJson(s, wdJson()); });
  add(S, sec, "/api/wd", HTTP_POST, L_ADMIN, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    cfg.wdTask = P(r, "task") == "1"; cfg.wdNet = P(r, "net") == "1"; cfg.wdRam = P(r, "ram") == "1";
    cfg.wdNetMin = constrain((int)P(r, "netmin").toInt(), 2, 1440);
    cfg.wdRamKb = constrain((int)P(r, "ramkb").toInt(), 8, 200);
    cfg.wdUpDays = constrain((int)P(r, "updays").toInt(), 0, 365);
    cfg.wdAt = constrain((int)P(r, "at").toInt(), -1, 1439);
    cfg.wdDays = P(r, "days").toInt() & 0x7F;
    cfgSave(); return ok(s);
  });

  // ---- servizi di rete: punto di accesso, HTTP, HTTPS, porte ----
  add(S, sec, "/api/svc", HTTP_GET, L_GUEST, [](Req* r, Res* s, Ctx& c) -> esp_err_t { return sendJson(s, svcJson()); });
  add(S, sec, "/api/svc", HTTP_POST, L_ADMIN, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    bool apOn = has(r, "apOn") ? P(r, "apOn") == "1" : cfg.apOn;
    bool cap = has(r, "captive") ? P(r, "captive") == "1" : cfg.apCaptive;
    bool dh = has(r, "dhcpOn") ? P(r, "dhcpOn") == "1" : cfg.dhcpOn;
    int lease = has(r, "lease") ? (int)P(r, "lease").toInt() : cfg.dhcpLease;
    bool md = has(r, "mdnsOn") ? P(r, "mdnsOn") == "1" : cfg.mdnsOn;
    if (lease < 10 || lease > 1440) return ko(s, tr("Durata dell'indirizzo: da 10 a 1440 minuti"));
    if (!dh && cfg.dhcpOn && netState() != NET_CLIENT_OK) return ko(s, tr("Spegni il DHCP dell'hotspot solo quando la scheda e collegata alla Wi-Fi di casa: senza DHCP il telefono dovrebbe avere un indirizzo fisso"));
    bool hOn = has(r, "httpOn") ? P(r, "httpOn") == "1" : cfg.httpOn;
    bool sOn = has(r, "httpsOn") ? P(r, "httpsOn") == "1" : cfg.https;
    int hp = has(r, "httpPort") ? (int)P(r, "httpPort").toInt() : cfg.httpPort;
    int sp = has(r, "httpsPort") ? (int)P(r, "httpsPort").toInt() : cfg.httpsPort;
    String e = cfgSvcCheck(apOn, hOn, hp, sOn, sp);
    if (e.length()) return ko(s, e);
    if (!apOn && cfg.apOn && netState() != NET_CLIENT_OK) return ko(s, tr("Spegni il Punto di accesso solo quando la scheda e collegata alla Wi-Fi di casa"));
    g_cleanArm = false;                                  // modifica a mano: la pulizia automatica non serve piu
    cfg.apOn = apOn; cfg.apCaptive = cap; cfg.httpOn = hOn; cfg.https = sOn; cfg.httpPort = hp; cfg.httpsPort = sp;
    cfg.dhcpOn = dh; cfg.dhcpLease = lease; cfg.mdnsOn = md;
    cfgSave(); netApplyServices();
    return sendJson(s, svcJson());
  });

  // ---- HTTPS ----
  add(S, sec, "/api/tls", HTTP_GET, L_GUEST, [](Req* r, Res* s, Ctx& c) -> esp_err_t { return sendJson(s, tlsJson()); });
  add(S, sec, "/api/tls/cert", HTTP_GET, L_GUEST, [](Req* r, Res* s, Ctx& c) -> esp_err_t {      // il certificato e pubblico: serve per "fidarsi" nel browser
    if (!tlsCertPem().length()) return ko(s, tr("Certificato non pronto"));
    s->addHeader("Content-Disposition", "attachment; filename=\"vesevos.crt\"");
    return sendText(s, 200, "application/x-pem-file", tlsCertPem());
  });
  add(S, sec, "/api/tls", HTTP_POST, L_ADMIN, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    if (has(r, "on")) {
      bool on = P(r, "on") == "1";
      String e = cfgSvcCheck(cfg.apOn, cfg.httpOn, cfg.httpPort, on, cfg.httpsPort);
      if (e.length()) return ko(s, e);
      cfg.https = on; cfgSave();
    }
    if (P(r, "regen") == "1") tlsRegenerate();
    if (P(r, "cert").length()) { String e; if (!tlsSetCustom(P(r, "cert"), P(r, "key"), e)) return ko(s, e); }
    return sendJson(s, tlsJson());
  });

  // ---- rete tra schede (ESP-NOW) ----
  add(S, sec, "/api/mesh", HTTP_GET, L_GUEST, [](Req* r, Res* s, Ctx& c) -> esp_err_t { return sendJson(s, meshJson()); });
  add(S, sec, "/api/mesh", HTTP_POST, L_ADMIN, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    int role = P(r, "role").toInt();
    if (role < 0 || role > 2) return ko(s, tr("Ruolo non valido"));
    cfg.meshRole = role; cfg.meshAuto = P(r, "auto") == "1";
    if (P(r, "newkey") == "1") cfg.meshKey = meshNewKey();
    else if (P(r, "key").length()) { String k = P(r, "key"); k.trim(); k.toLowerCase(); if (k.length() != 64) return ko(s, tr("La chiave deve avere 64 cifre esadecimali")); cfg.meshKey = k; }
    if (has(r, "ch")) { int ch = P(r, "ch").toInt(); if (!regionChannelOk(ch)) return ko(s, tr("Canale non ammesso nel paese scelto")); cfg.meshCh = ch; }
    cfgSave();
    if (meshRunning()) { String e; meshStart(e); }
    return sendJson(s, "{\"ok\":true,\"key\":\"" + cfg.meshKey + "\"}");
  });
  add(S, sec, "/api/mesh/key", HTTP_GET, L_ADMIN, [](Req* r, Res* s, Ctx& c) -> esp_err_t { return sendJson(s, "{\"key\":\"" + cfg.meshKey + "\"}"); });
  add(S, sec, "/api/mesh/run", HTTP_POST, L_ADMIN, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    String a = P(r, "a"), e;
    if (a == "start") { if (!meshStart(e)) return ko(s, e); }
    else if (a == "restart") { meshStop(); if (!meshStart(e)) return ko(s, e); }
    else if (a == "stop") meshStop();
    else return ko(s, tr("Azione non valida"));
    return ok(s);
  });
  add(S, sec, "/api/mesh/send", HTTP_POST, L_OPER, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    String e;
    bool okk = P(r, "cmd").length() ? meshSendCmd(P(r, "to"), P(r, "cmd"), e) : meshSendText(P(r, "to"), P(r, "text"), e);
    return okk ? ok(s) : ko(s, e);
  });

  // ---- Bluetooth (solo configurazione, 10 minuti) ----
  add(S, sec, "/api/ble", HTTP_GET, L_ADMIN, [](Req* r, Res* s, Ctx& c) -> esp_err_t { return sendJson(s, bleJson()); });
  add(S, sec, "/api/ble", HTTP_POST, L_ADMIN, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    String e;
    if (P(r, "a") == "start") { if (!bleStart(e)) return ko(s, e); }
    else bleStop();
    return sendJson(s, bleJson());
  });

  // ---- file (memoria interna) ----
  add(S, sec, "/api/fs/list", HTTP_GET, L_OPER, [](Req* r, Res* s, Ctx& c) -> esp_err_t { return sendJson(s, fsListJson(has(r, "path") ? P(r, "path") : String("/"))); });
  add(S, sec, "/api/fs/get", HTTP_GET, L_OPER, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    String p = fsClean(P(r, "path"));
    if (p.length() == 0 || fsProtected(p) || !LittleFS.exists(p)) return notFound(s);
    PsychicFileResponse f(s, LittleFS, p, "application/octet-stream", true);
    return f.send();
  });
  add(S, sec, "/api/fs/text", HTTP_GET, L_OPER, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    String t, err;
    if (!fsReadText(P(r, "path"), t, err)) return sendText(s, 404, "text/plain; charset=utf-8", err);
    return sendText(s, 200, "text/plain; charset=utf-8", t);
  });
  add(S, sec, "/api/fs/mkdir", HTTP_POST, L_ADMIN, [](Req* r, Res* s, Ctx& c) -> esp_err_t { String e; return fsMkdir(P(r, "path"), e) ? ok(s) : ko(s, e); });
  add(S, sec, "/api/fs/del", HTTP_POST, L_ADMIN, [](Req* r, Res* s, Ctx& c) -> esp_err_t { String e; return fsRemove(P(r, "path"), e) ? ok(s) : ko(s, e); });
  add(S, sec, "/api/fs/ren", HTTP_POST, L_ADMIN, [](Req* r, Res* s, Ctx& c) -> esp_err_t { String e; return fsRename(P(r, "from"), P(r, "to"), e) ? ok(s) : ko(s, e); });
  add(S, sec, "/api/fs/save", HTTP_POST, L_ADMIN, [](Req* r, Res* s, Ctx& c) -> esp_err_t { String e; return fsWriteText(P(r, "path"), P(r, "text"), e, P(r, "new") == "1") ? ok(s) : ko(s, e); });
  add(S, sec, "/api/fs/copy", HTTP_POST, L_ADMIN, [](Req* r, Res* s, Ctx& c) -> esp_err_t { String e; return fsCopy(P(r, "from"), P(r, "to"), e) ? ok(s) : ko(s, e); });
  add(S, sec, "/api/fs/dirs", HTTP_GET, L_OPER, [](Req* r, Res* s, Ctx& c) -> esp_err_t { return sendJson(s, fsDirsJson()); });

  // caricamento file: controllo d'accesso fatto anche durante l'invio dei dati
  PsychicUploadHandler* up = new PsychicUploadHandler();
  up->onUpload([sec](Req* r, const String& filename, uint64_t index, uint8_t* data, size_t len, bool final) -> esp_err_t {
    uint32_t ip = ipOf(r);
    if (!fwAllow(ip, false)) return ESP_FAIL;
    int u = authSessionUser(token(r));
    if (u < 0 || cfg.users[u].role < ROLE_ADMIN) return ESP_FAIL;
    if (!sec && g_tlsUp && cfg.https && !netOnAp(ip)) return ESP_FAIL;
    if (index == 0) {
      g_upOk = false; g_upErr = "";
      String dir = qparam(r, "dir"); if (!dir.length()) dir = "/";
      String name = filename; int sl = name.lastIndexOf('/'); if (sl >= 0) name = name.substring(sl + 1);
      String p = fsClean((dir == "/" ? String("") : dir) + "/" + name);
      if (p.length() == 0 || fsProtected(p)) { g_upErr = tr("Nome o percorso non valido"); return ESP_OK; }
      if (g_upFile) g_upFile.close();
      { int ls = p.lastIndexOf('/'); if (ls > 0) { String d = p.substring(0, ls); if (!LittleFS.exists(d)) LittleFS.mkdir(d); } }
      g_upFile = LittleFS.open(p, "w");
      if (!g_upFile) { g_upErr = tr("Impossibile creare il file"); return ESP_OK; }
      g_upPath = p;
    }
    if (g_upFile) {
      if (g_upFile.write(data, len) != len) { g_upErr = tr("Spazio esaurito"); g_upFile.close(); LittleFS.remove(g_upPath); return ESP_OK; }
      if (final) { g_upFile.close(); g_upOk = true; vlog("FILE: caricato %s", g_upPath.c_str()); }
    }
    return ESP_OK;
  });
  up->onRequest([](Req* r, Res* s) -> esp_err_t {
    int u = authSessionUser(token(r));
    if (u < 0 || cfg.users[u].role < ROLE_ADMIN) return sendJson(s, "{\"ok\":false,\"err\":\"non autorizzato\"}", 401);
    return g_upOk ? ok(s) : ko(s, g_upErr.length() ? g_upErr : String(tr("Caricamento non riuscito")));
  });
  S->on("/api/fs/up", HTTP_POST, up);

  // Portale automatico: in modo AP ogni indirizzo sconosciuto (controlli di Android, iPhone, Windows...) porta alla pagina.
  S->onNotFound([sec](Req* r, Res* s) -> esp_err_t {
    uint32_t ip = ipOf(r);
    if (!fwAllow(ip, false)) { fwNoteRejected(ip); return ESP_FAIL; }
    if (!sec && netCaptive() && !String(r->pathCStr()).startsWith("/api/")) return s->redirect("http://192.168.4.1/");
    return notFound(s);
  });
}

void webInit() {
  // intestazioni di sicurezza su tutte le risposte
  DefaultHeaders::Instance().addHeader("X-Content-Type-Options", "nosniff");
  DefaultHeaders::Instance().addHeader("X-Frame-Options", "DENY");
  DefaultHeaders::Instance().addHeader("Referrer-Policy", "no-referrer");
  DefaultHeaders::Instance().addHeader("Content-Security-Policy",
    "default-src 'self'; img-src 'self' data:; style-src 'self' 'unsafe-inline'; script-src 'self' 'unsafe-inline'; connect-src 'self'; frame-ancestors 'none'; base-uri 'none'; form-action 'self'");
  if (!authIsSet() && cfg.setupDone) g_firstPassUntil = millis() + 600000UL;   // reset con BOOT appena fatto
  vlog("WEB: percorsi pronti");
}

void webAllowFirstPass() { g_firstPassUntil = millis() + 600000UL; if (!g_firstPassUntil) g_firstPassUntil = 1; }

// Avvia i server solo quando la rete e gia inizializzata (chiamata dal task di rete)
void webStart() {
  static bool started = false;
  if (started) return;
  started = true;
  auto startHttp = [](uint16_t port) {
    g_http = new PsychicHttpServer(port);
    g_http->config.max_open_sockets = 3;               // basta per hotspot e portale automatico; HTTP e un server in piu, tiene poca RAM
    g_http->config.lru_purge_enable = true;            // quando i collegamenti sono finiti chiude il piu vecchio invece di rifiutare il nuovo
    g_http->maxRequestBodySize = 48 * 1024;
    routes(g_http, false);
    g_http->start();
    g_runHttpPort = port;
    vlog("WEB: server HTTP avviato sulla porta %u", (unsigned)port);
  };
  if (cfg.httpOn) startHttp(cfg.httpPort);
  else vlog("WEB: HTTP spento (scelta dell'utente)");
  if (cfg.https && tlsEnsure()) {
    g_https = new PsychicHttpsServer(cfg.httpsPort);
    g_https->ssl_config.httpd.max_open_sockets = 2;      // ogni connessione cifrata usa circa 40 KB di RAM
    g_https->ssl_config.httpd.lru_purge_enable = true;   // finiti i collegamenti chiude il piu vecchio invece di rifiutare il nuovo
    g_https->maxRequestBodySize = 48 * 1024;
    g_https->setCertificate(tlsCertPem().c_str(), tlsKeyPem().c_str());
    routes(g_https, true);
    g_tlsUp = g_https->start() == ESP_OK;
    if (g_tlsUp) { g_runHttpsPort = cfg.httpsPort; vlog("WEB: HTTPS avviato sulla porta %u (impronta %s...)", (unsigned)cfg.httpsPort, tlsFingerprint().substring(0, 23).c_str()); }
    else { vlog("WEB: HTTPS non partito, resta solo HTTP"); auditEvent(AUD_YELLOW, "httpsfail", tr("HTTPS non partito (memoria?): la pagina resta senza cifratura")); }
  }
  if (!g_http && !g_tlsUp) startHttp(cfg.httpPort);      // mai chiusi fuori: se HTTPS non parte e HTTP era spento, parte HTTP
}

bool webHttpsUp() { return g_tlsUp; }
