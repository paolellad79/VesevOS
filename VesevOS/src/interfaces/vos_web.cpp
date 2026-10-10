// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_web.cpp
// Server web con PsychicHttp (MIT), basato sul server ufficiale di ESP-IDF.
//  - porta 443 (HTTPS): pagina e API cifrate, certificato unico della scheda (vos_tls)
//  - porta 80  (HTTP):  portale automatico e, per chi e collegato all'hotspot (gia cifrato da WPA2), anche la pagina;
//                       dalla rete di casa porta a https://
// Ogni API ha un livello: pubblica, Ospite, Operatore, Amministratore (controllo nel firmware, non solo nella pagina).
// Il filtro IP (vos_fw) chiude la connessione senza risposta a chi non e ammesso.
#include "vos_web_int.h"
#include "../core/vos_ram.h"

namespace webx {
PsychicHttpServer* g_http = nullptr;
PsychicHttpsServer* g_https = nullptr;
bool g_tlsUp = false;
volatile bool g_cleanArm = false;   // fine guida con Wi-Fi di casa: al primo accesso dalla rete di casa si spengono hotspot e HTTP (solo in RAM)
uint16_t g_runHttpPort = 0, g_runHttpsPort = 0;   // porte con cui i server sono partiti (0 = non attivo)
uint32_t g_firstPassUntil = 0;     // dopo il reset con BOOT: 10 minuti per scegliere la password anche dalla rete di casa
File g_upFile;
bool g_upOk = false;
String g_upErr, g_upPath;


volatile uint32_t g_lastReq = 0;                       // ultima richiesta dalla pagina (per non dormire mentre la usi)

uint32_t ipOf(Req* r) {
  IPAddress a = r->client()->remoteIP();
  return ((uint32_t)a[0] << 24) | ((uint32_t)a[1] << 16) | ((uint32_t)a[2] << 8) | (uint32_t)a[3];
}

String P(Req* r, const char* name) {
  PsychicWebParameter* p = r->getParam(name);
  return p ? p->value() : String("");
}
bool has(Req* r, const char* name) { return r->hasParam(name); }

// parametro dalla riga dell'indirizzo (?nome=valore), anche nelle richieste di caricamento
String qparam(Req* r, const char* name) {
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

esp_err_t sendText(Res* s, int code, const char* type, const String& body) {
  s->setCode(code);
  s->setContentType(type);
  s->addHeader("Cache-Control", "no-store");
  s->setContent((const uint8_t*)body.c_str(), body.length());
  return s->send();
}
esp_err_t sendJson(Res* s, const String& body, int code) { return sendText(s, code, "application/json", body); }
esp_err_t ok(Res* s) { return sendJson(s, "{\"ok\":true}"); }
// errore: "err" e il testo tradotto per la pagina; "code" e il nome breve STABILE per app e prove (regole API 10).
// Il codice HTTP segue il "code" (regole API 11); il corpo ha sempre ok:false, quindi la pagina legge come prima.
// Attenzione: MAI 401 per gli errori di accesso (la pagina lo prende per "sessione scaduta" e rimostra il login).
static int httpFor(const char* code) {
  if (!strcmp(code, "blocked")) return 429;
  if (!strcmp(code, "forbidden") || !strcmp(code, "bad_login")) return 403;
  if (!strcmp(code, "notfound")) return 404;
  if (!strcmp(code, "state")) return 409;
  if (!strcmp(code, "toobig")) return 413;
  if (!strcmp(code, "off")) return 503;
  return 400;                                   // "error": dato mancante o non valido
}
esp_err_t ko(Res* s, const String& e, const char* code) { return sendJson(s, "{\"ok\":false,\"err\":\"" + jsonEscape(e) + "\",\"code\":\"" + code + "\"}", httpFor(code)); }
esp_err_t notFound(Res* s) { return sendText(s, 404, "text/plain; charset=utf-8", tr("Non trovato")); }

String token(Req* r) { return r->hasHeader("Cookie") ? authCookieFromHeader(r->header("Cookie")) : String(""); }

String who(const Ctx& c) {
  String n = c.user >= 0 ? cfg.users[c.user].name : String("-");
  return "web " + n + " " + ipToStr(c.ip);
}

// Registra un percorso con livello di accesso. secure = server HTTPS.

void add(PsychicHttpServer* srv, bool secure, const char* path, int method, uint8_t lvl, Handler h) {
  srv->on(path, method, [h, lvl, secure, method](Req* r, Res* s) -> esp_err_t {
    Ctx c; c.ip = ipOf(r); c.user = -1; c.role = 0; c.secure = secure;
    g_lastReq = millis();
    if (!fwAllow(c.ip, false)) { fwNoteRejected(c.ip); return ESP_FAIL; }     // chiude senza rispondere
    // dalla rete di casa la pagina e le API passano solo da HTTPS (l'hotspot e gia cifrato da WPA2)
    if (!secure && g_tlsUp && cfg.https && !netOnAp(c.ip)) {
      if (strcmp(r->pathCStr(), "/") == 0) { String host = r->host(); int col = host.indexOf(':'); if (col > 0) host = host.substring(0, col); return s->redirect(("https://" + host + (cfg.httpsPort != 443 ? ":" + String(cfg.httpsPort) : String("")) + "/").c_str()); }
      if (lvl != L_PUB || method == HTTP_POST) return sendJson(s, "{\"ok\":false,\"err\":\"https\",\"code\":\"https_only\"}", 403);   // anche accesso e prima password
    }
    if (lvl > L_PUB) {
      int u = authSessionUser(token(r));
      if (u < 0) { authNoteDenied(c.ip); return sendJson(s, "{\"ok\":false,\"err\":\"non autorizzato\",\"code\":\"unauthorized\"}", 401); }
      c.user = u; c.role = cfg.users[u].role;
      if (c.role + 1 < lvl) return sendJson(s, "{\"ok\":false,\"err\":\"" + jsonEscape(tr("Il tuo ruolo non permette questa operazione")) + "\",\"code\":\"forbidden\"}", 403);
    }
    if (method == HTTP_POST) cfgSetOrigin(who(c));
    return h(r, s, c);
  });
}

String svcJson() {
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

String settingsJson() {
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

void delayedRestart(void*) { vTaskDelay(pdMS_TO_TICKS(800)); ESP.restart(); }
volatile uint32_t g_sleepSec = 0;
void delayedSleep(void*) { vTaskDelay(pdMS_TO_TICKS(800)); if (g_sleepSec) powerSleepNow(g_sleepSec); else sysSleep(); }

esp_err_t sendPage(Res* s) {
  s->addHeader("Content-Encoding", "gzip");
  s->addHeader("Cache-Control", "no-store");
  return s->send(200, "text/html; charset=utf-8", INDEX_GZ, INDEX_GZ_LEN);
}

String cookieAttrs(bool secure) { return String("; Path=/; HttpOnly; SameSite=Strict") + (secure ? "; Secure" : ""); }

// dopo password (e codice MFA) giusti: sessione, cookie, pulizia di fine guida
esp_err_t loginDone(Res* s, Ctx& c, int idx) {
  String t = authNewSession(idx);
  vlog("WEB: accesso di %s da %s", cfg.users[idx].name.c_str(), ipToStr(c.ip).c_str());
  ramNote("login");
  bool cleaned = false;
  if (g_cleanArm && cfg.users[idx].role == ROLE_ADMIN && !netOnAp(c.ip) && netState() == NET_CLIENT_OK && cfg.staEnabled) {
    // la scheda e stata raggiunta dalla rete di casa: si spengono le cose servite solo all'hotspot
    g_cleanArm = false;
    cfg.apOn = false; cfg.apCaptive = false; cfg.dhcpOn = false;     // il DHCP serve solo all'hotspot: spento con lui (niente secondo DHCP sulla rete di casa)
    if (cfg.https && g_tlsUp) cfg.httpOn = false;       // mai chiusi fuori: HTTP si spegne solo se HTTPS funziona
    cfgSetOrigin("fine guida " + ipToStr(c.ip)); cfgSave(); cleaned = true;
    vlog("SETUP: raggiunta dalla rete di casa, spenti hotspot, DHCP e HTTP");
  }
  if (cfg.users[idx].mfaOn) cfgSave();                  // il contatore MFA non deve tornare indietro
  s->addHeader("Set-Cookie", ("vos=" + t + cookieAttrs(c.secure)).c_str());
  return sendJson(s, "{\"ok\":true,\"user\":\"" + jsonEscape(cfg.users[idx].name) + "\",\"role\":" + String(cfg.users[idx].role) + (cleaned ? ",\"clean\":true" : "") + "}");
}

}  // namespace webx

uint32_t webIdleSec() { return (millis() - g_lastReq) / 1000; }

// RAM a pezzi e nessuno usa la pagina da 20 s: si chiudono i collegamenti HTTPS rimasti aperti (ognuno tiene circa 40 KB).
// Il browser li riapre da solo quando serve. Chiamata dal monitor ogni 10 s.
void webTrimIdle() {
  if (!g_https || !g_tlsUp || !g_https->server) return;
  if (webIdleSec() < 20 || ESP.getMaxAllocHeap() >= 60000) return;
  int fds[8]; size_t n = 8;
  if (httpd_get_client_list(g_https->server, &n, fds) != ESP_OK) return;
  for (size_t i = 0; i < n; i++) httpd_sess_trigger_close(g_https->server, fds[i]);
  if (n) vlogl(LG_DBG, "WEB: RAM a pezzi, chiusi %u collegamenti HTTPS fermi", (unsigned)n);
}

// ---------------- tutti i percorsi (registrati su entrambi i server) ----------------
static void routesCore(PsychicHttpServer* S, bool sec) {
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
               ",\"ble\":" + (VOS_WITH_BLE ? "true" : "false") + ",\"pk\":{\"ble\":" + String(VOS_WITH_BLE) + ",\"mqtt\":" + String(VOS_WITH_MQTT) + ",\"mesh\":" + String(VOS_WITH_MESH) + ",\"mfa\":" + String(VOS_WITH_MFA) + ",\"stat\":" + String(VOS_WITH_STATS) + "}" + ",\"hp\":" + String(g_runHttpPort) + ",\"sp\":" + String(g_runHttpsPort) + timePubJson();
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
    if (code == "it" || !langCodeValid(code) || !drvFsExists(path)) return notFound(s);
    PsychicFileResponse f(s, drvFsHandle(), path, "application/json; charset=utf-8");
    return f.send();
  });
  add(S, sec, "/api/lang", HTTP_POST, L_ADMIN, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    String code = P(r, "code"); code.trim(); code.toLowerCase();
    String e;
    return langSet(code, e) ? ok(s) : ko(s, e);
  });

}

static void routes(PsychicHttpServer* S, bool sec) {
  routesCore(S, sec);
  routesAuth(S, sec);
  routesDev(S, sec);
  routesSys(S, sec);
  routesNet(S, sec);
  routesFiles(S, sec);
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
