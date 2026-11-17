// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_web.cpp
// Server web con PsychicHttp (MIT), basato sul server ufficiale di ESP-IDF.
//  - porta 443 (HTTPS): pagina e API cifrate, certificato unico della scheda (vos_tls)
//  - porta 80  (HTTP):  solo il redirect verso https:// (server minimo, vos_http_redirect_srv.cpp).
//                       Niente pagine in chiaro: se HTTPS non parte, la porta 80 resta chiusa (si resta dalla seriale).
// Ogni API ha un livello: pubblica, Ospite, Operatore, Amministratore (controllo nel firmware, non solo nella pagina).
// Il filtro IP (vos_fw) chiude la connessione senza risposta a chi non e ammesso.
#include "vos_web_int.h"
#include "vos_web_csrf.h"
#include "vos_http_redirect_srv.h"
#include "../core/vos_ram.h"
#include "../core/vos_risorse_tocco.h"
#include "../core/vos_primo_avvio.h"
#include "../core/vos_apwin.h"
#include "../drivers/vos_drv_time.h"
#include "../drivers/vos_drv_ip6.h"
#include "../drivers/vos_drv_heap.h"

namespace webx {
PsychicHttpsServer* g_https = nullptr;
bool g_tlsUp = false;
volatile bool g_cleanArm = false;   // fine guida con Wi-Fi di casa: al primo accesso dalla rete di casa si spengono hotspot e HTTP (solo in RAM)
uint16_t g_runHttpPort = 0, g_runHttpsPort = 0;   // porte con cui i server sono partiti (0 = non attivo)
uint32_t g_firstPassUntil = 0;     // dopo il reset con BOOT: 10 minuti per scegliere la password anche dalla rete di casa
VosFile g_upFile;
bool g_upOk = false;
ErrBuf g_upErr;
char g_upPath[160];


volatile uint32_t g_lastReq = 0;                       // ultima richiesta dalla pagina (per non dormire mentre la usi)

uint32_t ipOf(Req* r) {
  return drvSockPeer(r->client()->socket());   // IPv6 nativo: indirizzo finto (vedi drivers/vos_drv_ip6.h); prima si leggevano gli ultimi 4 byte, aggirabili dal filtro IP
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
// Riuscito, ma con qualcosa da dire a chi ha caricato il file: la pagina mostra "avviso".
esp_err_t okAvviso(Res* s, const ErrBuf& avviso) {
  if (avviso.empty()) return ok(s);
  HeapBuf j(512);
  j.add("{\"ok\":true,\"avviso\":\""); j.addJson(avviso.c_str(), avviso.length()); j.add("\"}");
  return j.oom() || j.over() ? ok(s) : sendJson(s, j);
}
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
// Le richieste che modificano devono venire dalla nostra pagina (o da un programma, non da un altro sito aperto nel browser)
bool csrfBlocked(Req* r) {
  char sf[24] = "", og[160] = "", hs[160] = "";                    // headerCStr() usa un solo buffer: ogni valore va copiato prima del prossimo
  if (r->hasHeader("Sec-Fetch-Site")) snprintf(sf, sizeof sf, "%s", r->headerCStr("Sec-Fetch-Site"));
  if (r->hasHeader("Origin")) snprintf(og, sizeof og, "%s", r->headerCStr("Origin"));
  if (r->hasHeader("Host")) snprintf(hs, sizeof hs, "%s", r->headerCStr("Host"));
  if (!csrfCrossSite(sf, og, hs)) return false;
  static uint32_t lastLog = 0;                                      // una riga ogni 10 s al massimo: una pagina ostile non deve riempire il registro
  if (!lastLog || drvMillis() - lastLog > 10000) { lastLog = drvMillis() | 1; vlog("WEB: richiesta di un altro sito rifiutata (%s)", r->pathCStr()); }
  return true;
}
int userOf(Req* r) {
  char tk[AUTH_TOKEN_BUF];
  tokenTo(r, tk, sizeof tk);
  int u = authSessionUser(tk);
  if (u >= 0) risAttore(cfg.users[u].name.c_str());          // chi scrive, per l'avviso su /usr/share/vesevos (core/vos_risorse_tocco.h)
  return u;
}

void whoTo(const Ctx& c, char* out, size_t n) {
  snprintf(out, n, "web %s %s", c.user >= 0 ? cfg.users[c.user].name.c_str() : "-", IpText(c.ip).s);
}

// add(), la tabella dei percorsi e lo smistamento delle richieste stanno in vos_web_smista.cpp.

// Testo corto di dimensione nota (< 400 byte): un solo blocco, consegnato come std::string
#if VOS_WITH_ELF
// Dettagli del servizio "elf" nello stato dei servizi: solo per l'Admin (la pagina mostra la scheda dei moduli ELF solo se ci sono).
static void svcElf(StrBuf& b, int role) {
  const ServiceOps* sv = serviceFind("elf");
  if (!sv || !sv->json || role < ROLE_ADMIN) return;
  HeapBuf h(256);
  if (sv->json(h) && !h.oom()) b.addf(",\"elf\":%s", h.c_str());
}
#endif

std::string svcJson(int role) {
  bool pend = (cfg.httpOn != (g_runHttpPort != 0)) || (cfg.httpOn && g_runHttpPort && cfg.httpPort != g_runHttpPort) ||
              (cfg.https != g_tlsUp) || (cfg.https && g_runHttpsPort && cfg.httpsPort != g_runHttpsPort);
  auto tf = [](bool v) { return v ? "true" : "false"; };
  StrText t(512);
  t.buf().addf("{\"apOn\":%s,\"captive\":%s,\"dhcpOn\":%s,\"lease\":%d,\"mdnsOn\":%s,", tf(cfg.apOn), tf(cfg.apCaptive), tf(cfg.dhcpOn), (int)cfg.dhcpLease, tf(cfg.mdnsOn));
  t.buf().addf("\"httpOn\":%s,\"httpPort\":%d,\"httpsOn\":%s,\"httpsPort\":%d,", tf(cfg.httpOn), (int)cfg.httpPort, tf(cfg.https), (int)cfg.httpsPort);
  t.buf().addf("\"runHttp\":%u,\"runHttps\":%u,\"pending\":%s,\"sta\":%s,", (unsigned)g_runHttpPort, (unsigned)g_runHttpsPort, tf(pend), tf(cfg.staEnabled && cfg.staSsid.length()));
  t.buf().addf("\"clientOk\":%s,\"usersHttp\":%d,\"usersHttps\":%d", tf(netState() == NET_CLIENT_OK), (int)authSessionCount(0), (int)authSessionCount(1));
#if VOS_WITH_ELF
  svcElf(t.buf(), role);
#endif
  t.buf().add("}");
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
  j.add(",\"alias\":");                                  // nome vero -> {a: alias, d: descrizione} di ogni periferica (decisione B del 13/11/2026)
  aliasJson(j);
  return j.add('}') && !j.oom() && !j.over();
}

void delayedRestart(void* why) { vTaskDelay(pdMS_TO_TICKS(800)); sysReboot(why ? (const char*)why : "pagina", 0); }   // why = testo fisso (letterale)
volatile uint32_t g_sleepSec = 0;
void delayedSleep(void*) { vTaskDelay(pdMS_TO_TICKS(800)); if (g_sleepSec) powerSleepNow(g_sleepSec); else sysSleep(); }


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
    // HTTP (solo redirect verso HTTPS) resta acceso: decisione dell'utente 15/11/2026
    cfgSetOriginf("fine guida %s", IpText(c.ip).s); cfgSave(); cleaned = true;
    vlog("SETUP: raggiunta dalla rete di casa, spenti hotspot e DHCP");
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
static void webRegistraRotteRestanti();

void webTrimIdle() {
  webRegistraRotteRestanti();                        // riprova se la RAM non bastava (solo dopo che il task di rete ha dato il via)
  if (!g_https || !g_tlsUp || !g_https->server) return;
  if (webIdleSec() < 20 || drvHeapMaxBlock() >= 60000) return;
  int fds[8]; size_t n = 8;
  if (httpd_get_client_list(g_https->server, &n, fds) != ESP_OK) return;
  for (size_t i = 0; i < n; i++) httpd_sess_trigger_close(g_https->server, fds[i]);
  if (n) vlogl(LG_DBG, "WEB: RAM a pezzi, chiusi %u collegamenti HTTPS fermi", (unsigned)n);
}

// ---------------- tutti i percorsi (registrati su entrambi i server) ----------------
static void routesCore(PsychicHttpServer* S, bool sec) {
  add(S, sec, "/", HTTP_GET, L_PUB, [](Req* r, Res* s, Ctx& c) -> esp_err_t { return sendPage(s); });   // la pagina da /usr/share/vesevos/html.vrs, o quella di emergenza se il file manca o non e valido (vos_web_pagina.cpp)

  // dati comuni (paesi, mappa, regole radio, elenco librerie): pubblici, uguali per tutti, conservati dal browser
  add(S, sec, "/api/common", HTTP_GET, L_PUB, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    esp_err_t err = ESP_OK;
    if (sendRisFile(s, RF_COMUNI, "application/json", true, "public, max-age=86400", err)) return err;
    return sendText(s, 503, "text/plain; charset=utf-8", tr("Dati comuni non disponibili: il file manca o non e valido"));   // la pagina mostra solo il paese salvato
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
#if VOS_WITH_ELF
    jsonBool(j, "elf", true);                                         // il firmware ha i moduli ELF: la pagina mostra la scheda (assente = non c'e)
#endif
    char pk[160];
    snprintf(pk, sizeof pk, ",\"pk\":{\"ble\":%d,\"mqtt\":%d,\"mesh\":%d,\"mfa\":%d,\"stat\":%d,\"wps\":%d,\"script\":%d,\"app\":%d},\"hp\":%u,\"sp\":%u",
             VOS_WITH_BLE, VOS_WITH_MQTT, VOS_WITH_MESH, VOS_WITH_MFA, VOS_WITH_STATS, VOS_WITH_WPS, VOS_WITH_FUNZIONI, VOS_WITH_FUNZIONI, (unsigned)g_runHttpPort, (unsigned)g_runHttpsPort);
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
    int rf = licFile(li);                                                // licenza minore: sta in un file di /usr/share/vesevos (gzip)
    if (rf >= 0) {
      esp_err_t err = ESP_OK;
      if (sendRisFile(s, (RisFile)rf, "text/plain; charset=utf-8", true, nullptr, err)) return err;
      return sendText(s, 200, "text/plain; charset=utf-8", tr("Il testo di questa licenza sta in un file della scheda che manca o non e valido. Le note legali complete restano disponibili (id notice) e nel file LICENSE del progetto."));
    }
    if (licZ(li)) { s->addHeader("Content-Encoding", "gzip"); return s->send(200, "text/plain; charset=utf-8", (const uint8_t*)licText(li), licZ(li)); }   // GPL-3.0: compressa, il browser la apre
    return s->send(200, "text/plain; charset=utf-8", (const uint8_t*)licText(li), licSize(li));
  });

  // Lingue (pubbliche: servono anche alla schermata di accesso, contengono solo testi)
  add(S, sec, "/api/langs", HTTP_GET, L_PUB, [](Req* r, Res* s, Ctx& c) -> esp_err_t { HeapBuf j(LANG_JSON_MAX); return langListJson(j) ? sendJson(s, j) : ko(s, tr("Memoria insufficiente")); });
  add(S, sec, "/api/langfile", HTTP_GET, L_PUB, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    const char* code = Pc(r, "code");
    char path[48];                                                      // "/lang/" + codice (max 8 caratteri) + ".vlp"
    if (strlen(code) > LANG_CODE_BUF - 1 || strcmp(code, "it") == 0 || !langCodeValid(code)) return notFound(s);
    snprintf(path, sizeof path, "/lang/%s.vlp", code);                  // frasi della pagina: contenuto gzip, lo apre il browser
    if (!drvFsExists(path)) return notFound(s);
    s->addHeader("Content-Encoding", "gzip");
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
  routesSys(S, sec);
  routesNet(S, sec);
  routesFiles(S, sec);
  routesScript(S, sec);
  routesProg(S, sec);
#if VOS_WITH_ELF
  routesElf(S, sec);
#endif
  routesApp(S, sec);
  routesUpd(S, sec);
  routesAgg(S, sec);
  routesEventi(S, sec);
  if (rottaSi("/api/risorse/carica", HTTP_POST)) routeRisorse(S, sec);
}

void webInit() {
  // intestazioni di sicurezza su tutte le risposte
  DefaultHeaders::Instance().addHeader("X-Content-Type-Options", "nosniff");
  DefaultHeaders::Instance().addHeader("X-Frame-Options", "DENY");
  DefaultHeaders::Instance().addHeader("Referrer-Policy", "no-referrer");
  DefaultHeaders::Instance().addHeader("Content-Security-Policy",
    "default-src 'self'; img-src 'self' data:; style-src 'self' 'unsafe-inline'; script-src 'self' 'unsafe-inline'; connect-src 'self' https://raw.githubusercontent.com; frame-ancestors 'none'; base-uri 'none'; form-action 'self'");
  if (!authIsSet() && cfg.setupDone) g_firstPassUntil = drvMillis() + 600000UL;   // reset con BOOT appena fatto
  vlog("WEB: percorsi pronti");
}

void webAllowFirstPass() { g_firstPassUntil = drvMillis() + 600000UL; if (!g_firstPassUntil) g_firstPassUntil = 1; }

// Porta HTTP: solo il redirect verso HTTPS (server minimo, vos_http_redirect_srv.cpp). Parte solo se HTTPS e in funzione.
static void startHttpRedirect(uint16_t port) {
  if (httpRedirectSrvStart(port)) {
    g_runHttpPort = port;
    vlog("WEB: HTTP solo redirect verso HTTPS sulla porta %u", (unsigned)port);
  } else {
    vlog("WEB: %s", tr("server HTTP (redirect) non partito: porta occupata o memoria"));
  }
}

// Diagnostica delle connessioni HTTPS: una riga per apertura e una per chiusura, con quante sono aperte e la RAM, senza IP.
// Usa le funzioni utente di PsychicHttp (non tocca open_fn e close_fn, che sono suoi). Una chiusura senza apertura
// (riga "psychic: No client record") e una stretta di mano TLS fallita.
static unsigned connContate() {
  int fds[8];
  size_t n = 8;
  if (!g_https) return 0;
  if (httpd_get_client_list(g_https->server, &n, fds) != ESP_OK) return 0;
  return (unsigned)n;
}

static void connAperta(PsychicClient* c) {
  vlog("WEB: connessione aperta fd %d, aperte %u, RAM libera %u KB, blocco %u KB", c->socket(), connContate(),
       (unsigned)(drvHeapFree() / 1024), (unsigned)(drvHeapMaxBlock() / 1024));
  ramNote("tls-conn");
}

static void connChiusa(PsychicClient* c) {
  unsigned n = connContate();               // la sessione che si chiude e ancora nell'elenco
  vlog("WEB: connessione chiusa fd %d, aperte %u", c->socket(), n ? n - 1 : 0);
}

// Server HTTPS: parte una volta sola (la scelta del server HTTP dipende dal suo esito, g_tlsUp)
static void startHttps() {
  ramTrace("web-pre");
  if (!(cfg.https && tlsEnsure())) return;
  ramTrace("tls-ok");
  g_https = new PsychicHttpsServer(cfg.httpsPort);
  // Due connessioni sono il minimo che la pagina regge: il browser ne tiene aperte piu di una e con max_open_sockets = 1 ogni
  // nuova chiudeva la precedente a meta risposta (guida iniziale sulla C3). 2 e una STIMA della RAM: circa 23 KB per sessione
  // TLS attiva, 64 KB liberi a https avviato sulla C3; 3 non entrano. Il valore si corregge sulle righe "WEB: connessione"
  // (esp_http_server riserva 3 socket e lwIP vuole LWIP_MAX_SOCKETS >= max_open_sockets + 3).
  g_https->ssl_config.httpd.max_open_sockets = 2;
  g_https->ssl_config.httpd.lru_purge_enable = true;   // finiti i collegamenti chiude il piu vecchio invece di rifiutare il nuovo
  g_https->onOpen(connAperta);                         // diagnostica: una riga per apertura e per chiusura (vedi sopra)
  g_https->onClose(connChiusa);
  g_https->maxRequestBodySize = 48 * 1024;
  g_https->maxUploadSize = 4UL * 1024 * 1024;
  g_https->setCertificate(tlsCertPem(), tlsKeyPem());
  g_rotteModo = primoAvvioMinimo() ? ROTTE_GUIDA : ROTTE_TUTTE;   // finestra della guida: solo i percorsi della guida (risparmia circa 35 KB sulla C3)
  routes(g_https, true);
  ramTrace(g_rotteModo == ROTTE_GUIDA ? "rotte-guida" : "rotte");
  g_tlsUp = g_https->start() == ESP_OK;
  ramTrace("https");
  if (g_tlsUp) { g_runHttpsPort = cfg.httpsPort; vlog("WEB: HTTPS avviato sulla porta %u (impronta %.23s...)", (unsigned)cfg.httpsPort, tlsFingerprint()); }
  else { vlog("WEB: HTTPS non partito, resta solo HTTP"); auditEvent(AUD_YELLOW, "httpsfail", tr("HTTPS non partito (memoria?): la pagina resta senza cifratura")); }
}

// A guida finita registra i percorsi rimandati. PsychicHttp li cerca in un suo elenco (nel server di IDF restano solo i
// percorsi "*" per metodo, GET e POST gia presenti), quindi si possono aggiungere a server avviato. Una volta sola.
// Prima controlla la RAM: la creazione di ogni percorso non sopporta di restare senza memoria.
static const uint32_t RAM_PER_ROTTE_RESTANTI = 40000;   // misura sulla C3 (1.8.23e): i 151 percorsi restanti costano circa 35 KB, piu un margine

static bool g_rotteVia = false;                       // il task di rete ha detto: la guida e finita davvero (ritardo passato), si puo registrare

static void webRegistraRotteRestanti() {
  if (g_rotteModo != ROTTE_GUIDA || !g_https || !g_tlsUp || !g_rotteVia) return;
  if (primoAvvioMinimo()) return;                    // la guida non e ancora finita
  if (drvHeapFree() < RAM_PER_ROTTE_RESTANTI) {
    static uint32_t ultimoAvviso = 0;
    if (!ultimoAvviso || drvMillis() - ultimoAvviso > 60000) { ultimoAvviso = drvMillis() | 1; vlog("WEB: RAM insufficiente per i percorsi rimandati, riprovo"); }
    return;
  }
  g_rotteModo = ROTTE_RESTANTI;                      // prima del giro: un secondo chiamante esce subito
  g_rotteConta = 0;
  routes(g_https, true);
  g_rotteModo = ROTTE_TUTTE;
  ramTrace("rotte-resto");
  vlog("WEB: rotte restanti registrate (%d), in tabella %d", g_rotteConta, rotteInTabella());
}

// Fine guida, dal task di rete: registra i percorsi rimandati PRIMA che partano HTTP, DNS del portale e mDNS (costano circa 22 KB).
// true = tutti registrati (o niente da fare); false = RAM scarsa, il task di rete riprova ogni secondo e intanto non avvia altro.
bool webRotte() {
  g_rotteVia = true;
  webRegistraRotteRestanti();
  return g_rotteModo != ROTTE_GUIDA || !g_https || !g_tlsUp;
}

// Avvia i server solo quando la rete e gia inizializzata (chiamata dal task di rete).
// Si puo richiamare: l'HTTPS parte la prima volta, il redirect HTTP subito dopo (anche nella finestra della prima accensione).
void webStart() {
  static bool httpsTried = false;
  if (!httpsTried) { httpsTried = true; startHttps(); }
  if (!primoAvvioMinimo() && !primoAvvioPendente()) g_rotteVia = true;   // guida finita da un pezzo (anche con la radio spenta): si puo registrare
  webRegistraRotteRestanti();                        // guida finita: arrivano anche tutti gli altri percorsi
  if (g_rotteModo == ROTTE_GUIDA && g_https && g_tlsUp && !primoAvvioMinimo()) return;   // percorsi ancora da registrare: HTTP resta spento finche non ci sono
  // Prima accensione: il redirect HTTP parte anche nella finestra della guida (serve al portale automatico: i telefoni provano la porta 80;
  // decisione dell'utente del 15/11/2026). Il blocco sopra non vale qui: in finestra le rotte restanti non si registrano.
  if (g_runHttpPort) return;                                 // il redirect e gia partito
  if (!cfg.httpOn) vlog("WEB: HTTP spento (scelta dell'utente)");
  else if (g_tlsUp) { startHttpRedirect(cfg.httpPort); ramTrace("http"); }
  else vlog("WEB: HTTPS non in funzione: porta HTTP chiusa, resta solo la seriale");
}

bool webHttpsUp() { return g_tlsUp; }
