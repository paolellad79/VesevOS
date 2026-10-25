// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_web.cpp
// Server web con PsychicHttp (MIT), basato sul server ufficiale di ESP-IDF.
//  - porta 443 (HTTPS): pagina e API cifrate, certificato unico della scheda (vos_tls)
//  - porta 80  (HTTP):  portale automatico e, per chi e collegato all'hotspot (gia cifrato da WPA2), anche la pagina;
//                       dalla rete di casa porta a https://
//                       Con il redirect acceso e l'hotspot che non puo accendersi, la porta 80 la tiene un server MINIMO (solo redirect,
//                       vos_http_redirect_srv.cpp) invece di questo server completo: risparmia RAM. Scelta in startHttp().
// Ogni API ha un livello: pubblica, Ospite, Operatore, Amministratore (controllo nel firmware, non solo nella pagina).
// Il filtro IP (vos_fw) chiude la connessione senza risposta a chi non e ammesso.
#include "vos_web_int.h"
#include "vos_redirect_plan.h"
#include "vos_http_redirect_srv.h"
#include "../core/vos_ram.h"
#include "../drivers/vos_drv_time.h"
#include "../drivers/vos_drv_heap.h"

namespace webx {
PsychicHttpServer* g_http = nullptr;
PsychicHttpsServer* g_https = nullptr;
bool g_tlsUp = false;
volatile bool g_cleanArm = false;   // fine guida con Wi-Fi di casa: al primo accesso dalla rete di casa si spengono hotspot e HTTP (solo in RAM)
uint16_t g_runHttpPort = 0, g_runHttpsPort = 0;   // porte con cui i server sono partiti (0 = non attivo)
bool g_httpMini = false;           // la porta HTTP e tenuta dal server minimo (solo redirect)
bool g_httpMiniWanted = false;     // cosa aveva deciso httpMinimalWanted() all'avvio (se cambia dopo, serve il riavvio)
uint32_t g_firstPassUntil = 0;     // dopo il reset con BOOT: 10 minuti per scegliere la password anche dalla rete di casa
VosFile g_upFile;
bool g_upOk = false;
ErrBuf g_upErr;
char g_upPath[160];


volatile uint32_t g_lastReq = 0;                       // ultima richiesta dalla pagina (per non dormire mentre la usi)

uint32_t ipOf(Req* r) {
  return ipNum(r->client()->remoteIP());
}

const char* Pc(Req* r, const char* name) {
  PsychicWebParameter* p = r->getParam(name);
  return p ? p->valueCStr() : "";
}
bool Pis(Req* r, const char* name, const char* value) { return strcmp(Pc(r, name), value) == 0; }
long Pint(Req* r, const char* name) { return atol(Pc(r, name)); }
bool has(Req* r, const char* name) { return r->hasParam(name); }

std::string_view Pt(Req* r, const char* name) { return trimView(Pc(r, name)); }

// parametro dalla riga dell'indirizzo (?nome=valore), anche nelle richieste di caricamento
bool qparamTo(Req* r, const char* name, char* out, size_t cap) {
  out[0] = 0;
  const char* q = r->queryCStr();
  size_t nl = strlen(name);
  while (*q) {
    const char* e = strchr(q, '&');
    size_t len = e ? (size_t)(e - q) : strlen(q);
    if (len > nl && strncmp(q, name, nl) == 0 && q[nl] == '=') {
      std::string raw(q + nl + 1, len - nl - 1);          // urlDecode vuole un testo chiuso con 0
      auto dec = urlDecode(raw.c_str());
      snprintf(out, cap, "%s", dec.c_str());
      return true;
    }
    if (!e) break;
    q = e + 1;
  }
  return false;
}

esp_err_t sendText(Res* s, int code, const char* type, const char* body, size_t len) {
  s->setCode(code);
  s->setContentType(type);
  s->addHeader("Cache-Control", "no-store");
  s->setContent((const uint8_t*)body, len);
  return s->send();
}
esp_err_t sendText(Res* s, int code, const char* type, const char* body) { return sendText(s, code, type, body, strlen(body)); }
esp_err_t sendJson(Res* s, const char* body, int code) { return sendText(s, code, "application/json", body, strlen(body)); }
esp_err_t sendJson(Res* s, const HeapBuf& body, int code) { return sendText(s, code, "application/json", body.c_str(), body.length()); }
esp_err_t sendJsonBuf(Res* s, const std::string& body, int code) { return sendText(s, code, "application/json", body.data(), body.size()); }
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
esp_err_t ko(Res* s, const char* e, const char* code) {
  HeapBuf j(2048);
  j.add("{\"ok\":false,\"err\":\""); j.addJson(e, strlen(e)); j.add("\",\"code\":\""); j.add(code); j.add("\"}");
  if (j.oom() || j.over()) return sendText(s, httpFor(code), "application/json", "{\"ok\":false,\"err\":\"?\",\"code\":\"error\"}", 38);
  return sendText(s, httpFor(code), "application/json", j.c_str(), j.length());
}
esp_err_t notFound(Res* s) { return sendText(s, 404, "text/plain; charset=utf-8", tr("Non trovato")); }

void tokenTo(Req* r, char* out, size_t cap) {
  out[0] = 0;
  if (r->hasHeader("Cookie")) authCookieFromHeader(r->headerCStr("Cookie"), out, cap);
}
int userOf(Req* r) { char tk[AUTH_TOKEN_BUF]; tokenTo(r, tk, sizeof tk); return authSessionUser(tk); }

void whoTo(const Ctx& c, char* out, size_t n) {
  snprintf(out, n, "web %s %s", c.user >= 0 ? cfg.users[c.user].name.c_str() : "-", IpText(c.ip).s);
}

// Registra un percorso con livello di accesso. secure = server HTTPS.

void add(PsychicHttpServer* srv, bool secure, const char* path, int method, uint8_t lvl, Handler h) {
  srv->on(path, method, [h, lvl, secure, method](Req* r, Res* s) -> esp_err_t {
    Ctx c; c.ip = ipOf(r); c.user = -1; c.role = 0; c.secure = secure;
    g_lastReq = drvMillis();
    if (!fwAllow(c.ip, false)) { fwNoteRejected(c.ip); return ESP_FAIL; }     // chiude senza rispondere
    if (!secure && redirectActive(c.ip)) return redirectReply(r, s);         // impostazione redirect: tutto verso HTTPS, niente servito su HTTP
    // dalla rete di casa la pagina e le API passano solo da HTTPS (l'hotspot e gia cifrato da WPA2)
    if (!secure && g_tlsUp && cfg.https && !netOnAp(c.ip)) {
      if (strcmp(r->pathCStr(), "/") == 0) {                                 // alla pagina HTTPS, stesso nome ma senza la porta
        const char* host = r->headerCStr("Host");
        size_t hl = host ? strcspn(host, ":") : 0;
        char url[160], port[8] = "";
        if (cfg.httpsPort != 443) snprintf(port, sizeof port, ":%u", (unsigned)cfg.httpsPort);
        snprintf(url, sizeof url, "https://%.*s%s/", (int)hl, host ? host : "", port);
        return s->redirect(url);
      }
      if (lvl != L_PUB || method == HTTP_POST) return sendJson(s, "{\"ok\":false,\"err\":\"https\",\"code\":\"https_only\"}", 403);   // anche accesso e prima password
    }
    if (lvl > L_PUB) {
      char tk[AUTH_TOKEN_BUF]; tokenTo(r, tk, sizeof tk);
      int u = authSessionUser(tk);
      if (u < 0) { authNoteDenied(c.ip); return sendJson(s, "{\"ok\":false,\"err\":\"non autorizzato\",\"code\":\"unauthorized\"}", 401); }
      c.user = u; c.role = cfg.users[u].role;
      authSessionNote(tk, c.ip, secure, method == HTTP_POST, r->pathCStr());   // per "chi e collegato" (who)
      if (c.role + 1 < lvl) return ko(s, tr("Il tuo ruolo non permette questa operazione"), "forbidden");   // HTTP 403
    }
    if (method == HTTP_POST) { char w[96]; whoTo(c, w, sizeof w); cfgSetOrigin(w); }
    return h(r, s, c);
  });
}

// Testo corto di dimensione nota (< 400 byte): un solo blocco, consegnato come std::string
std::string svcJson() {
  bool pend = (cfg.httpOn != (g_runHttpPort != 0)) || (cfg.httpOn && g_runHttpPort && cfg.httpPort != g_runHttpPort) ||
              (cfg.https != g_tlsUp) || (cfg.https && g_runHttpsPort && cfg.httpsPort != g_runHttpsPort) ||
              (g_runHttpPort && httpMinimalWanted() != g_httpMiniWanted);   // redirect o hotspot cambiati: il server HTTP giusto si sceglie al riavvio
  auto tf = [](bool v) { return v ? "true" : "false"; };
  StrText t(512);
  t.buf().addf("{\"apOn\":%s,\"captive\":%s,\"dhcpOn\":%s,\"lease\":%d,\"mdnsOn\":%s,", tf(cfg.apOn), tf(cfg.apCaptive), tf(cfg.dhcpOn), (int)cfg.dhcpLease, tf(cfg.mdnsOn));
  t.buf().addf("\"httpOn\":%s,\"httpPort\":%d,\"httpsOn\":%s,\"httpsPort\":%d,", tf(cfg.httpOn), (int)cfg.httpPort, tf(cfg.https), (int)cfg.httpsPort);
  t.buf().addf("\"runHttp\":%u,\"runHttps\":%u,\"pending\":%s,\"sta\":%s,", (unsigned)g_runHttpPort, (unsigned)g_runHttpsPort, tf(pend), tf(cfg.staEnabled && cfg.staSsid.length()));
  t.buf().addf("\"clientOk\":%s,\"usersHttp\":%d,\"usersHttps\":%d,\"redirect\":%s,\"httpMin\":%s}", tf(netState() == NET_CLIENT_OK), (int)authSessionCount(0), (int)authSessionCount(1), tf(cfg.httpRedirect), tf(g_httpMini));
  return t.take();
}

bool settingsJson(HeapBuf& j) {
  j.add("{\"hostname\":\""); j.addJson(cfg.hostname.c_str(), cfg.hostname.length());
  j.add("\"");
  jsonStr(j, "domain", cfg.domain.c_str());
  jsonBool(j, "serialAuth", cfg.serialAuth);
  jsonStr(j, "staSsid", cfg.staSsid.c_str());
  jsonBool(j, "staDhcp", cfg.staDhcp);
  jsonStr(j, "ip", cfg.ip.c_str()); jsonStr(j, "mask", cfg.mask.c_str());
  jsonStr(j, "gw", cfg.gw.c_str()); jsonStr(j, "dns1", cfg.dns1.c_str()); jsonStr(j, "dns2", cfg.dns2.c_str());
  jsonStr(j, "apSsid", cfg.apSsid.c_str());
  jsonNum(j, "ledMode", cfg.ledMode); jsonNum(j, "ledColor", cfg.ledColor); jsonNum(j, "ledBrightness", cfg.ledBrightness);
  for (int i = 0; i < LEDSRC_COUNT; i++) { char key[12]; snprintf(key, sizeof key, "ledSrc%d", i); jsonNum(j, key, cfg.ledSrcColor[i]); }
  jsonNum(j, "ledCpuThr", cfg.ledCpuThr);
  return j.add('}');
}

void delayedRestart(void* why) { vTaskDelay(pdMS_TO_TICKS(800)); sysReboot(why ? (const char*)why : "pagina", 0); }   // why = testo fisso (letterale)
volatile uint32_t g_sleepSec = 0;
void delayedSleep(void*) { vTaskDelay(pdMS_TO_TICKS(800)); if (g_sleepSec) powerSleepNow(g_sleepSec); else sysSleep(); }

esp_err_t sendPage(Res* s) {
  s->addHeader("Content-Encoding", "gzip");
  s->addHeader("Cache-Control", "no-store");
  return s->send(200, "text/html; charset=utf-8", INDEX_GZ, INDEX_GZ_LEN);
}


// dopo password (e codice MFA) giusti: sessione, cookie, pulizia di fine guida
esp_err_t loginDone(Res* s, Ctx& c, int idx) {
  char t[AUTH_TOKEN_BUF];
  authNewSession(t, idx, c.ip, c.secure);
  vlog("WEB: accesso di %s da %s", cfg.users[idx].name.c_str(), IpText(c.ip).s);
  ramNote("login");
  bool cleaned = false;
  if (g_cleanArm && cfg.users[idx].role == ROLE_ADMIN && !netOnAp(c.ip) && netState() == NET_CLIENT_OK && cfg.staEnabled) {
    // la scheda e stata raggiunta dalla rete di casa: si spengono le cose servite solo all'hotspot
    g_cleanArm = false;
    cfg.apOn = false; cfg.apCaptive = false; cfg.dhcpOn = false;     // il DHCP serve solo all'hotspot: spento con lui (niente secondo DHCP sulla rete di casa)
    if (cfg.https && g_tlsUp) cfg.httpOn = false;       // mai chiusi fuori: HTTP si spegne solo se HTTPS funziona
    cfgSetOriginf("fine guida %s", IpText(c.ip).s); cfgSave(); cleaned = true;
    vlog("SETUP: raggiunta dalla rete di casa, spenti hotspot, DHCP e HTTP");
  }
  if (cfg.users[idx].mfaOn) cfgSave();                  // il contatore MFA non deve tornare indietro
  char ck[160];
  snprintf(ck, sizeof ck, "vos=%s" COOKIE_ATTRS "%s", t, c.secure ? "; Secure" : "");
  s->addHeader("Set-Cookie", ck);
  HeapBuf j(512);
  j.add("{\"ok\":true,\"user\":\""); j.addJson(cfg.users[idx].name.c_str(), cfg.users[idx].name.length());
  j.add('"'); jsonNum(j, "role", cfg.users[idx].role);
  if (cleaned) j.add(",\"clean\":true");
  j.add('}');
  if (j.oom() || j.over()) return ko(s, tr("Memoria insufficiente"), "off");
  return sendJson(s, j);
}

}  // namespace webx

uint32_t webIdleSec() { return (drvMillis() - g_lastReq) / 1000; }

// RAM a pezzi e nessuno usa la pagina da 20 s: si chiudono i collegamenti HTTPS rimasti aperti (ognuno tiene circa 40 KB).
// Il browser li riapre da solo quando serve. Chiamata dal monitor ogni 10 s.
void webTrimIdle() {
  if (!g_https || !g_tlsUp || !g_https->server) return;
  if (webIdleSec() < 20 || drvHeapMaxBlock() >= 60000) return;
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
    int u = userOf(r);
    HeapBuf j(2048);
    j.add("{\"set\":"); j.add(authIsSet() ? "true" : "false");
    jsonBool(j, "setup", cfg.setupDone);
    jsonStr(j, "lang", cfg.lang.c_str());
    jsonBool(j, "secure", c.secure);
    jsonBool(j, "https", g_tlsUp && cfg.https);
    jsonStr(j, "fp", g_tlsUp ? tlsFingerprint() : "");
    jsonBool(j, "ble", VOS_WITH_BLE);
    char pk[160];
    snprintf(pk, sizeof pk, ",\"pk\":{\"ble\":%d,\"mqtt\":%d,\"mesh\":%d,\"mfa\":%d,\"stat\":%d,\"wps\":%d,\"script\":%d,\"app\":%d},\"hp\":%u,\"sp\":%u",
             VOS_WITH_BLE, VOS_WITH_MQTT, VOS_WITH_MESH, VOS_WITH_MFA, VOS_WITH_STATS, VOS_WITH_WPS, VOS_WITH_BERRY, VOS_WITH_BERRY, (unsigned)g_runHttpPort, (unsigned)g_runHttpsPort);
    j.add(pk);
    timePubJson(j);                                                   // pezzo JSON gia pronto dall'ora (comincia con la virgola)
    if (!authIsSet()) { int a = authFirstAdmin(); jsonStr(j, "first", a >= 0 ? cfg.users[a].name.c_str() : "admin"); }
    if (u >= 0) { jsonStr(j, "user", cfg.users[u].name.c_str()); jsonNum(j, "role", cfg.users[u].role); }
    j.add('}');
    if (j.oom() || j.over()) return ko(s, tr("Memoria insufficiente"), "off");
    return sendJson(s, j);
  });

  // Licenze e note legali (pubbliche, leggibili anche prima dell'accesso)
  add(S, sec, "/api/license", HTTP_GET, L_PUB, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    if (!has(r, "id")) {
      char lst[LIC_JSON_MAX];
      licListJson(lst, sizeof(lst));
      return sendJson(s, lst);
    }
    int li = licFind(Pc(r, "id"));
    if (li < 0) return notFound(s);
    if (licZ(li)) { s->addHeader("Content-Encoding", "gzip"); return s->send(200, "text/plain; charset=utf-8", (const uint8_t*)licText(li), licZ(li)); }   // licenze lunghe: compresse, il browser le apre
    return s->send(200, "text/plain; charset=utf-8", (const uint8_t*)licText(li), licSize(li));
  });

  // Lingue (pubbliche: servono anche alla schermata di accesso, contengono solo testi)
  add(S, sec, "/api/langs", HTTP_GET, L_PUB, [](Req* r, Res* s, Ctx& c) -> esp_err_t { HeapBuf j(LANG_JSON_MAX); return langListJson(j) ? sendJson(s, j) : ko(s, tr("Memoria insufficiente")); });
  add(S, sec, "/api/langfile", HTTP_GET, L_PUB, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    const char* code = Pc(r, "code");
    char path[48];                                                      // "/lang/" + codice (max 8 caratteri) + ".json"
    if (strlen(code) > LANG_CODE_BUF - 1 || strcmp(code, "it") == 0 || !langCodeValid(code)) return notFound(s);
    snprintf(path, sizeof path, "/lang/%s.json", code);
    if (!drvFsExists(path)) return notFound(s);
    return sendFile(s, path, "application/json; charset=utf-8", false);
  });
  add(S, sec, "/api/lang", HTTP_POST, L_ADMIN, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    std::string_view v = Pt(r, "code");
    char code[LANG_CODE_BUF + 3];                                       // piu lungo di 8 resta non valido anche tagliato
    size_t n = v.size() < sizeof(code) - 1 ? v.size() : sizeof(code) - 1;
    for (size_t i = 0; i < n; i++) code[i] = (char)tolower((unsigned char)v[i]);
    code[n] = 0;
    ErrBuf e;
    return langSet(code, e) ? ok(s) : ko(s, e);
  });

}

static void routes(PsychicHttpServer* S, bool sec) {
  routesCore(S, sec);
  routesAuth(S, sec);
  routesDev(S, sec);
#if VOS_WITH_SD
  routesSd(S, sec);
#endif
  routesSys(S, sec);
  routesNet(S, sec);
  routesFiles(S, sec);
  routesScript(S, sec);
  routesApp(S, sec);
  routesUpd(S, sec);
}

void webInit() {
  // intestazioni di sicurezza su tutte le risposte
  DefaultHeaders::Instance().addHeader("X-Content-Type-Options", "nosniff");
  DefaultHeaders::Instance().addHeader("X-Frame-Options", "DENY");
  DefaultHeaders::Instance().addHeader("Referrer-Policy", "no-referrer");
  DefaultHeaders::Instance().addHeader("Content-Security-Policy",
    "default-src 'self'; img-src 'self' data:; style-src 'self' 'unsafe-inline'; script-src 'self' 'unsafe-inline'; connect-src 'self'; frame-ancestors 'none'; base-uri 'none'; form-action 'self'");
  if (!authIsSet() && cfg.setupDone) g_firstPassUntil = drvMillis() + 600000UL;   // reset con BOOT appena fatto
  vlog("WEB: percorsi pronti");
}

void webAllowFirstPass() { g_firstPassUntil = drvMillis() + 600000UL; if (!g_firstPassUntil) g_firstPassUntil = 1; }

// Server HTTP COMPLETO (Psychic): pagina, API e portale automatico. Tiene molta RAM (task httpd da 8 KB, tutte le rotte).
static void startHttpFull(uint16_t port) {
  g_http = new PsychicHttpServer(port);
  g_http->config.max_open_sockets = 3;               // basta per hotspot e portale automatico; HTTP e un server in piu, tiene poca RAM
  g_http->config.lru_purge_enable = true;            // quando i collegamenti sono finiti chiude il piu vecchio invece di rifiutare il nuovo
  g_http->maxRequestBodySize = 48 * 1024;
  g_http->maxUploadSize = 4UL * 1024 * 1024;         // oltre questo la libreria risponde in inglese e chiude: i limiti veri li controllano le rotte (messaggi tradotti)
  routes(g_http, false);
  g_http->onNotFound(httpNotFound);
  g_http->start();
  g_runHttpPort = port;
  vlog("WEB: server HTTP completo avviato sulla porta %u", (unsigned)port);
}

// Sceglie chi tiene la porta HTTP: il server MINIMO (solo redirect) se httpMinimalWanted(), altrimenti il completo. Se il minimo non parte
// (porta occupata o memoria) si ripiega sul completo: la porta HTTP non resta mai senza risposta. La scelta si fa qui, all'avvio dei server:
// un cambio dopo (redirect, hotspot) vale al riavvio e svcJson() lo segnala come "pending".
static void startHttp(uint16_t port) {
  g_httpMiniWanted = httpMinimalWanted();
  if (g_httpMiniWanted) {
    if (httpRedirectSrvStart(port)) {
      g_httpMini = true;
      g_runHttpPort = port;
      vlog("WEB: server HTTP minimo (solo redirect a HTTPS) avviato sulla porta %u", (unsigned)port);
      return;
    }
    vlog("WEB: %s", tr("server HTTP minimo non partito (porta occupata o memoria), uso il server completo"));
  }
  startHttpFull(port);
}

// Avvia i server solo quando la rete e gia inizializzata (chiamata dal task di rete)
void webStart() {
  static bool started = false;
  if (started) return;
  started = true;
  // HTTPS per primo: la scelta del server HTTP dipende dal suo esito (g_tlsUp)
  if (cfg.https && tlsEnsure()) {
    g_https = new PsychicHttpsServer(cfg.httpsPort);
    g_https->ssl_config.httpd.max_open_sockets = 2;      // ogni connessione cifrata usa circa 40 KB di RAM
    g_https->ssl_config.httpd.lru_purge_enable = true;   // finiti i collegamenti chiude il piu vecchio invece di rifiutare il nuovo
    g_https->maxRequestBodySize = 48 * 1024;
    g_https->maxUploadSize = 4UL * 1024 * 1024;
    g_https->setCertificate(tlsCertPem(), tlsKeyPem());
    routes(g_https, true);
    g_tlsUp = g_https->start() == ESP_OK;
    if (g_tlsUp) { g_runHttpsPort = cfg.httpsPort; vlog("WEB: HTTPS avviato sulla porta %u (impronta %.23s...)", (unsigned)cfg.httpsPort, tlsFingerprint()); }
    else { vlog("WEB: HTTPS non partito, resta solo HTTP"); auditEvent(AUD_YELLOW, "httpsfail", tr("HTTPS non partito (memoria?): la pagina resta senza cifratura")); }
  }
  if (cfg.httpOn) startHttp(cfg.httpPort);
  else vlog("WEB: HTTP spento (scelta dell'utente)");
  if (!g_runHttpPort && !g_tlsUp) startHttp(cfg.httpPort);      // mai chiusi fuori: se HTTPS non parte e HTTP era spento, parte HTTP
}

bool webHttpsUp() { return g_tlsUp; }
