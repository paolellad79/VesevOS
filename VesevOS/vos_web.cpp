// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_web.cpp
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
#include <LittleFS.h>
#include "vos_log.h"
#include <ESPAsyncWebServer.h>

static AsyncWebServer server(80);
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

static String token(AsyncWebServerRequest* r) {
  if (!r->hasHeader("Cookie")) return "";
  return authCookieFromHeader(r->header("Cookie"));
}

static bool checkAuth(AsyncWebServerRequest* r) {
  if (authSessionValid(token(r))) return true;
  authNoteDenied((uint32_t)r->remoteIP());
  r->send(401, "application/json", "{\"ok\":false,\"err\":\"non autorizzato\"}");
  return false;
}

static String P(AsyncWebServerRequest* r, const char* name) {
  if (r->hasParam(name, true)) return r->getParam(name, true)->value();
  return "";
}

static void sendJson(AsyncWebServerRequest* r, const String& body, int code = 200) {
  AsyncWebServerResponse* resp = r->beginResponse(code, "application/json", body);
  resp->addHeader("Cache-Control", "no-store");
  r->send(resp);
}

static void ok(AsyncWebServerRequest* r) { sendJson(r, "{\"ok\":true}"); }
static void ko(AsyncWebServerRequest* r, const String& e) { sendJson(r, "{\"ok\":false,\"err\":\"" + jsonEscape(e) + "\"}"); }

static String settingsJson() {
  String j = "{";
  j += "\"hostname\":\"" + jsonEscape(cfg.hostname) + "\",\"domain\":\"" + jsonEscape(cfg.domain) + "\",";
  j += "\"serialAuth\":" + String(cfg.serialAuth ? "true" : "false") + ",";
  j += "\"staSsid\":\"" + jsonEscape(cfg.staSsid) + "\",";
  j += "\"staDhcp\":" + String(cfg.staDhcp ? "true" : "false") + ",";
  j += "\"ip\":\"" + jsonEscape(cfg.ip) + "\",\"mask\":\"" + jsonEscape(cfg.mask) + "\",";
  j += "\"gw\":\"" + jsonEscape(cfg.gw) + "\",\"dns1\":\"" + jsonEscape(cfg.dns1) + "\",\"dns2\":\"" + jsonEscape(cfg.dns2) + "\",";
  j += "\"ledMode\":" + String(cfg.ledMode) + ",\"ledColor\":" + String(cfg.ledColor) + ",\"ledBrightness\":" + String(cfg.ledBrightness);
  j += "}";
  return j;
}

static void delayedRestart(void*) { vTaskDelay(pdMS_TO_TICKS(800)); ESP.restart(); }
static void delayedSleep(void*) { vTaskDelay(pdMS_TO_TICKS(800)); sysSleep(); }

void webInit() {
  // Pagina (senza autenticazione: contiene solo il login)
  server.on("/", HTTP_GET, [](AsyncWebServerRequest* r) {
    AsyncWebServerResponse* resp = r->beginResponse(200, "text/html; charset=utf-8", INDEX_HTML);
    resp->addHeader("Cache-Control", "no-store");
    r->send(resp);
  });

  server.on("/api/auth", HTTP_GET, [](AsyncWebServerRequest* r) {
    sendJson(r, String("{\"set\":") + (authIsSet() ? "true" : "false") + ",\"lang\":\"" + jsonEscape(cfg.lang) + "\"}");
  });

  // Licenze e note legali (senza autenticazione: sono testi pubblici, leggibili anche prima del login)
  server.on("/api/license", HTTP_GET, [](AsyncWebServerRequest* r) {
    if (!r->hasParam("id")) { sendJson(r, licListJson()); return; }
    int li = licFind(r->getParam("id")->value());
    if (li < 0) { r->send(404, "text/plain", tr("Non trovato")); return; }
    AsyncWebServerResponse* resp = r->beginResponse(200, "text/plain; charset=utf-8", (const uint8_t*)licText(li), licSize(li));
    r->send(resp);
  });

  // Lingue (senza autenticazione: servono anche alla schermata di accesso, contengono solo testi)
  server.on("/api/langs", HTTP_GET, [](AsyncWebServerRequest* r) { sendJson(r, langListJson()); });
  server.on("/api/langfile", HTTP_GET, [](AsyncWebServerRequest* r) {
    String c = r->hasParam("code") ? r->getParam("code")->value() : String("");
    String path = "/lang/" + c + ".json";
    if (c == "it" || !langCodeValid(c) || !LittleFS.exists(path)) { r->send(404, "text/plain", tr("Non trovato")); return; }
    AsyncWebServerResponse* resp = r->beginResponse(LittleFS, path, "application/json; charset=utf-8");
    resp->addHeader("Cache-Control", "no-cache");
    r->send(resp);
  });
  server.on("/api/lang", HTTP_POST, [](AsyncWebServerRequest* r) {
    if (!checkAuth(r)) return;
    String c = P(r, "code"); c.trim(); c.toLowerCase();
    String e;
    if (langSet(c, e)) ok(r); else ko(r, e);
  });

  server.on("/api/login", HTTP_POST, [](AsyncWebServerRequest* r) {
    String p = P(r, "p");
    if (!authIsSet()) {                           // primo accesso: crea la password
      if (p.length() < 6) { ko(r, tr("Password troppo corta (min 6)")); return; }
      authSetPassword(p); cfgSave();
    } else {
      uint32_t wait = 0;
      int res = authCheckFrom((uint32_t)r->remoteIP(), p, wait);
      if (res == 2) { ko(r, trf("Troppi errori da questo indirizzo: riprova tra %lu secondi", (unsigned long)wait)); return; }
      if (res != 0) { ko(r, tr("Password errata")); return; }
    }
    String t = authNewSession();
    AsyncWebServerResponse* resp = r->beginResponse(200, "application/json", "{\"ok\":true}");
    resp->addHeader("Set-Cookie", "vos=" + t + "; Path=/; HttpOnly; SameSite=Strict");
    r->send(resp);
  });

  server.on("/api/ban", HTTP_GET, [](AsyncWebServerRequest* r) { if (checkAuth(r)) sendJson(r, authBanJson()); });
  server.on("/api/ban/unban", HTTP_POST, [](AsyncWebServerRequest* r) { if (!checkAuth(r)) return; authUnban(P(r, "ip")); ok(r); });
  server.on("/api/ban/set", HTTP_POST, [](AsyncWebServerRequest* r) {
    if (!checkAuth(r)) return;
    int f = P(r, "fails").toInt(); long sc = P(r, "secs").toInt();
    if (f < 3 || f > 20 || sc < 10 || sc > 3600) { ko(r, tr("Valori non validi")); return; }
    cfg.banFails = f; cfg.banSecs = sc; cfgSave(); ok(r);
  });
  server.on("/api/logout", HTTP_POST, [](AsyncWebServerRequest* r) {
    authLogout(token(r));
    AsyncWebServerResponse* resp = r->beginResponse(200, "application/json", "{\"ok\":true}");
    resp->addHeader("Set-Cookie", "vos=; Path=/; Max-Age=0");
    r->send(resp);
  });

  server.on("/api/status", HTTP_GET, [](AsyncWebServerRequest* r) { if (checkAuth(r)) sendJson(r, sysStatusJson()); });
  server.on("/api/settings", HTTP_GET, [](AsyncWebServerRequest* r) { if (checkAuth(r)) sendJson(r, settingsJson()); });
  server.on("/api/pins", HTTP_GET, [](AsyncWebServerRequest* r) { if (checkAuth(r)) sendJson(r, pinsJson()); });
  server.on("/api/pins/info", HTTP_GET, [](AsyncWebServerRequest* r) { if (checkAuth(r)) sendJson(r, pinBoardJson()); });
  server.on("/api/pinmap", HTTP_GET, [](AsyncWebServerRequest* r) { if (checkAuth(r)) sendJson(r, pinMapJson()); });
  server.on("/api/pintest", HTTP_GET, [](AsyncWebServerRequest* r) { if (checkAuth(r)) sendJson(r, pinTestJson()); });
  server.on("/api/pintest", HTTP_POST, [](AsyncWebServerRequest* r) {
    if (!checkAuth(r)) return;
    String err;
    if (!pinTestRequest(P(r, "gpio").toInt(), P(r, "action"), P(r, "pull"), err)) { ko(r, err); return; }
    ok(r);
  });
  server.on("/api/log", HTTP_GET, [](AsyncWebServerRequest* r) {
    if (!checkAuth(r)) return;
    r->send(200, "text/plain; charset=utf-8", logGet(150));
  });
  server.on("/api/log/clear", HTTP_POST, [](AsyncWebServerRequest* r) { if (!checkAuth(r)) return; logClear(); ok(r); });
  server.on("/api/tasks", HTTP_GET, [](AsyncWebServerRequest* r) {
    if (!checkAuth(r)) return;
    r->send(200, "text/plain; charset=utf-8", sysTasksText());
  });

  server.on("/api/serialauth", HTTP_POST, [](AsyncWebServerRequest* r) {
    if (!checkAuth(r)) return;
    cfg.serialAuth = (P(r, "on") != "0");
    cfgSave();
    vlog("SICUREZZA: password sulla seriale %s", cfg.serialAuth ? "attivata" : "disattivata");
    ok(r);
  });
  server.on("/api/identify", HTTP_POST, [](AsyncWebServerRequest* r) { if (!checkAuth(r)) return; ledIdentify(10000); ok(r); });
  server.on("/api/tasklist", HTTP_GET, [](AsyncWebServerRequest* r) { if (checkAuth(r)) sendJson(r, sysTasksJson()); });
  server.on("/api/taskkill", HTTP_POST, [](AsyncWebServerRequest* r) {
    if (!checkAuth(r)) return;
    String err;
    if (sysTaskKill(P(r, "name"), err)) ok(r); else ko(r, err);
  });
  server.on("/api/taskrestart", HTTP_POST, [](AsyncWebServerRequest* r) {
    if (!checkAuth(r)) return;
    String err;
    if (sysTaskRestart(P(r, "name"), err)) ok(r); else ko(r, err);
  });

  // Automazioni
  server.on("/api/rules", HTTP_GET, [](AsyncWebServerRequest* r) {
    if (!checkAuth(r)) return;
    sendJson(r, "{\"text\":\"" + jsonEscape(rulesText()) + "\",\"st\":" + rulesStatusJson() + "}");
  });
  server.on("/api/rules", HTTP_POST, [](AsyncWebServerRequest* r) {
    if (!checkAuth(r)) return;
    String e;
    if (rulesSave(P(r, "text"), e)) ok(r); else ko(r, e);
  });
  server.on("/api/rules/run", HTTP_POST, [](AsyncWebServerRequest* r) {
    if (!checkAuth(r)) return;
    String e;
    if (rulesRunNow(P(r, "i").toInt(), e)) ok(r); else ko(r, e);
  });
  // Ordine di avvio
  server.on("/api/boot", HTTP_GET, [](AsyncWebServerRequest* r) { if (checkAuth(r)) sendJson(r, bootJson()); });
  server.on("/api/boot", HTTP_POST, [](AsyncWebServerRequest* r) {
    if (!checkAuth(r)) return;
    String res, e;
    if (bootSetOrder(P(r, "order"), res, e)) sendJson(r, "{\"ok\":true,\"order\":\"" + jsonEscape(res) + "\"}"); else ko(r, e);
  });
  server.on("/api/boot/reset", HTTP_POST, [](AsyncWebServerRequest* r) { if (!checkAuth(r)) return; bootResetOrder(); ok(r); });
  server.on("/api/wifi/scan", HTTP_GET, [](AsyncWebServerRequest* r) { if (checkAuth(r)) sendJson(r, netScanJson()); });
  server.on("/api/wifi/scan", HTTP_POST, [](AsyncWebServerRequest* r) { if (checkAuth(r)) { netScanStart(); ok(r); } });

  server.on("/api/wifi/save", HTTP_POST, [](AsyncWebServerRequest* r) {
    if (!checkAuth(r)) return;
    String ssid = P(r, "ssid"), pass = P(r, "pass");
    bool dhcp = P(r, "dhcp") != "0";
    if (ssid.length() == 0 || ssid.length() > 32) { ko(r, tr("Nome rete non valido")); return; }
    if (pass.length() > 0 && (pass.length() < 8 || pass.length() > 63)) { ko(r, tr("Password Wi-Fi: da 8 a 63 caratteri")); return; }
    if (!dhcp) {
      uint32_t a, m, g, d;
      if (!ipParse(P(r, "ip"), a)) { ko(r, tr("IP non valido")); return; }
      if (!ipParse(P(r, "mask"), m) || !maskValid(m)) { ko(r, tr("Subnet mask non valida")); return; }
      if (!ipParse(P(r, "gw"), g)) { ko(r, tr("Gateway non valido")); return; }
      if (((a ^ g) & m) != 0) { ko(r, tr("Il gateway non e nella stessa rete dell'IP")); return; }
      if (P(r, "d1").length() && !ipParse(P(r, "d1"), d)) { ko(r, tr("DNS 1 non valido")); return; }
      if (P(r, "d2").length() && !ipParse(P(r, "d2"), d)) { ko(r, tr("DNS 2 non valido")); return; }
      cfg.ip = P(r, "ip"); cfg.mask = P(r, "mask"); cfg.gw = P(r, "gw");
      cfg.dns1 = P(r, "d1"); cfg.dns2 = P(r, "d2");
    }
    cfg.staSsid = ssid;
    if (pass.length()) cfg.staPass = pass;
    cfg.staDhcp = dhcp; cfg.staEnabled = true;
    cfgSave(); netReconfigure(); ok(r);
  });

  server.on("/api/wifi/ap", HTTP_POST, [](AsyncWebServerRequest* r) {
    if (!checkAuth(r)) return;
    cfg.staEnabled = false; cfgSave(); netReconfigure(); ok(r);
  });

  server.on("/api/shell", HTTP_POST, [](AsyncWebServerRequest* r) {
    if (!checkAuth(r)) return;
    String c = P(r, "c");
    if (c.length() > 200) { r->send(200, "text/plain; charset=utf-8", tr("Comando troppo lungo")); return; }
    StrPrint sp;
    shellExec(c, sp, true);
    r->send(200, "text/plain; charset=utf-8", cleanAscii(sp.s));
  });

  server.on("/api/led", HTTP_POST, [](AsyncWebServerRequest* r) {
    if (!checkAuth(r)) return;
    int m = P(r, "mode").toInt(), b = P(r, "br").toInt();
    if (m < 0 || m > 3 || b < 0 || b > 255) { ko(r, tr("Valori non validi")); return; }
    cfg.ledMode = m; cfg.ledBrightness = b;
    cfg.ledColor = (uint32_t)strtoul(P(r, "color").c_str(), NULL, 10) & 0xFFFFFF;
    ledApplyConfig(); cfgSave(); ok(r);
  });

  server.on("/api/system", HTTP_POST, [](AsyncWebServerRequest* r) {
    if (!checkAuth(r)) return;
    String hn = P(r, "hostname"), dm = P(r, "domain");
    hn.trim(); dm.trim(); hn.toLowerCase(); dm.toLowerCase();
    if (!hostnameValid(hn)) { ko(r, tr("Nome host non valido (1-32 caratteri: lettere, numeri, trattino; non all'inizio o alla fine)")); return; }
    if (!domainValid(dm)) { ko(r, tr("Dominio non valido (nomi separati da punti, solo lettere, numeri e trattino)")); return; }
    bool changed = (hn != cfg.hostname);
    cfg.hostname = hn; cfg.domain = dm;
    cfgSave();
    if (changed) netReconfigure();
    ok(r);
  });

  server.on("/api/passwd", HTTP_POST, [](AsyncWebServerRequest* r) {
    if (!checkAuth(r)) return;
    if (!authCheck(P(r, "o"))) { ko(r, tr("Vecchia password errata")); return; }
    String n = P(r, "n");
    if (n.length() < 6) { ko(r, tr("Nuova password troppo corta (min 6)")); return; }
    authSetPassword(n); cfgSave(); ok(r);
  });

  server.on("/api/config/download", HTTP_GET, [](AsyncWebServerRequest* r) {
    if (!checkAuth(r)) return;
    AsyncWebServerResponse* resp = r->beginResponse(200, "text/plain; charset=utf-8", cfgExport(false));
    resp->addHeader("Content-Disposition", "attachment; filename=\"vesevos.conf\"");
    r->send(resp);
  });

  server.on("/api/config/restore", HTTP_POST, [](AsyncWebServerRequest* r) {
    if (!checkAuth(r)) return;
    String t = P(r, "t"), err;
    if (t.length() == 0 || t.length() > 8000) { ko(r, tr("File vuoto o troppo grande")); return; }
    if (!cfgImport(t, err)) { ko(r, err); return; }
    cfgSave(); ledApplyConfig(); netReconfigure(); ok(r);
  });

  server.on("/api/airplane", HTTP_POST, [](AsyncWebServerRequest* r) {
    if (!checkAuth(r)) return;
    if (P(r, "on") != "1") { netAirplaneOff("dalla pagina"); ok(r); return; }
    String err;
    if (netAirplaneOn(P(r, "exit").toInt(), strtoul(P(r, "param").c_str(), NULL, 10), err)) ok(r); else ko(r, err);
  });
  // ---- MQTT ----
  server.on("/api/mqtt", HTTP_GET, [](AsyncWebServerRequest* r) { if (checkAuth(r)) sendJson(r, mqttStatusJson()); });
  server.on("/api/mqtt", HTTP_POST, [](AsyncWebServerRequest* r) {
    if (!checkAuth(r)) return;
    String host = P(r, "host"), pre = P(r, "prefix"), user = P(r, "user");
    long port = P(r, "port").toInt(), ev = P(r, "every").toInt();
    host.trim(); pre.trim(); user.trim();
    if (host.length() > 80 || user.length() > 60 || pre.length() > 60) { ko(r, tr("Valori troppo lunghi")); return; }
    for (size_t i = 0; i < host.length(); i++) { char c = host[i]; if (!(isAlphaNumeric(c) || c == '.' || c == '-')) { ko(r, tr("Broker: solo lettere, numeri, punto e trattino")); return; } }
    for (size_t i = 0; i < pre.length(); i++) { char c = pre[i]; if (c == '#' || c == '+' || c < 33 || c > 126) { ko(r, tr("Prefisso: niente spazi, # o +")); return; } }
    if (port < 1 || port > 65535 || ev < 5 || ev > 3600) { ko(r, tr("Valori non validi")); return; }
    cfg.mqttHost = host; cfg.mqttPort = port; cfg.mqttUser = user; cfg.mqttPrefix = pre; cfg.mqttEvery = ev;
    if (r->hasParam("pass", true) && P(r, "pass").length()) cfg.mqttPass = P(r, "pass");
    if (P(r, "clearpass") == "1") cfg.mqttPass = "";
    cfg.mqttAuto = P(r, "auto") == "1"; cfg.mqttHa = P(r, "ha") == "1";
    cfgSave();
    if (mqttRunning()) mqttStart();      // riparte con i valori nuovi
    ok(r);
  });
  server.on("/api/mqtt/run", HTTP_POST, [](AsyncWebServerRequest* r) {
    if (!checkAuth(r)) return;
    String a = P(r, "a");
    if (a == "start" || a == "restart") { if (!cfg.mqttHost.length()) { ko(r, tr("Manca l'indirizzo del broker")); return; } mqttStart(); }
    else if (a == "stop") mqttStop();
    else if (a == "test") { if (!mqttPublishRel("test", "VesevOS " VOS_VERSION)) { ko(r, tr("MQTT non collegato")); return; } }
    else { ko(r, tr("Azione non valida")); return; }
    ok(r);
  });
  server.on("/api/reboot", HTTP_POST, [](AsyncWebServerRequest* r) {
    if (!checkAuth(r)) return;
    vlog("SISTEMA: riavvio dalla pagina"); ok(r);
    xTaskCreate(delayedRestart, "rst", 2048, NULL, 1, NULL);
  });
  server.on("/api/sleep", HTTP_POST, [](AsyncWebServerRequest* r) {
    if (!checkAuth(r)) return;
    ok(r);
    xTaskCreate(delayedSleep, "slp", 3072, NULL, 1, NULL);
  });

  server.on("/api/factory", HTTP_POST, [](AsyncWebServerRequest* r) {
    if (!checkAuth(r)) return;
    cfgFactoryReset(); ok(r);
    xTaskCreate(delayedRestart, "rst", 2048, NULL, 1, NULL);
  });

  // ---- ora / NTP ----
  server.on("/api/time", HTTP_GET, [](AsyncWebServerRequest* r) { if (checkAuth(r)) sendJson(r, timeJson()); });
  server.on("/api/time", HTTP_POST, [](AsyncWebServerRequest* r) {
    if (!checkAuth(r)) return;
    String srv = P(r, "server"), tz = P(r, "tz"), tzn = P(r, "tzname");
    if (srv.length() < 1 || srv.length() > 60) { ko(r, tr("Server NTP non valido")); return; }
    for (size_t i = 0; i < srv.length(); i++) { char c = srv[i]; if (!(isAlphaNumeric(c) || c == '.' || c == '-')) { ko(r, tr("Server NTP: solo lettere, numeri, punto e trattino")); return; } }
    if (tz.length() < 3 || tz.length() > 60) { ko(r, tr("Fuso orario non valido")); return; }
    for (size_t i = 0; i < tz.length(); i++) { unsigned char c = tz[i]; if (c < 32 || c >= 127 || c == '\'' || c == '"') { ko(r, tr("Fuso orario: caratteri non validi")); return; } }
    int df = P(r, "datefmt").toInt(), tf = P(r, "timefmt").toInt(), tu = P(r, "tempunit").toInt();
    if (df < 0 || df > 2 || tf < 0 || tf > 1 || tu < 0 || tu > 1) { ko(r, tr("Formato non valido")); return; }
    cfg.dateFmt = df; cfg.timeFmt = tf; cfg.tempUnit = tu;
    long ev = r->hasParam("every", true) ? P(r, "every").toInt() : (long)cfg.ntpEvery;
    if (!timeEveryValid(ev)) { ko(r, tr("Frequenza non valida")); return; }
    cfg.ntpEvery = ev;
    cfg.ntpOn = P(r, "ntp") == "1"; cfg.ntpServe = P(r, "serve") == "1";
    cfg.ntpServer = srv; cfg.tz = tz; cfg.tzName = tzn.length() ? cleanAscii(tzn) : String("Personalizzato");
    setenv("TZ", cfg.tz.c_str(), 1); tzset();
    cfgSave(); timeApply(); ok(r);
  });
  server.on("/api/time/sync", HTTP_POST, [](AsyncWebServerRequest* r) {
    if (!checkAuth(r)) return;
    if (!cfg.ntpOn) { ko(r, tr("La sincronizzazione NTP e spenta")); return; }
    if (netState() != NET_CLIENT_OK) { ko(r, tr("Serve la connessione a una rete Wi-Fi")); return; }
    timeApply(); vlog("TIME: sincronizzazione richiesta dalla pagina"); ok(r);
  });
  server.on("/api/time/set", HTTP_POST, [](AsyncWebServerRequest* r) {
    if (!checkAuth(r)) return;
    if (r->hasParam("local", true)) { String err; if (timeSetLocal(P(r, "local"), err)) ok(r); else ko(r, err); return; }
    unsigned long e = strtoul(P(r, "epoch").c_str(), NULL, 10);
    if (e < 1700000000UL) { ko(r, tr("Ora non valida")); return; }
    timeSetEpoch(e); ok(r);
  });
  server.on("/api/cpu", HTTP_POST, [](AsyncWebServerRequest* r) {
    if (!checkAuth(r)) return;
    int m = P(r, "mode").toInt();
    if (m != 0 && m != 80 && m != 160 && m != 240) { ko(r, tr("Valore non valido")); return; }
    cfg.cpuMhz = m; cfgSave(); sysApplyCpuMode(); ok(r);
  });

  // ---- file (memoria interna) ----
  server.on("/api/fs/list", HTTP_GET, [](AsyncWebServerRequest* r) {
    if (!checkAuth(r)) return;
    String p = r->hasParam("path") ? r->getParam("path")->value() : String("/");
    sendJson(r, fsListJson(p));
  });
  server.on("/api/fs/get", HTTP_GET, [](AsyncWebServerRequest* r) {
    if (!checkAuth(r)) return;
    String p = fsClean(r->hasParam("path") ? r->getParam("path")->value() : String(""));
    if (p.length() == 0 || fsProtected(p) || !LittleFS.exists(p)) { r->send(404, "text/plain", tr("Non trovato")); return; }
    r->send(LittleFS, p, "application/octet-stream", true);
  });
  server.on("/api/fs/text", HTTP_GET, [](AsyncWebServerRequest* r) {
    if (!checkAuth(r)) return;
    String t, err;
    if (!fsReadText(r->hasParam("path") ? r->getParam("path")->value() : String(""), t, err)) { r->send(404, "text/plain; charset=utf-8", err); return; }
    r->send(200, "text/plain; charset=utf-8", t);
  });
  server.on("/api/fs/mkdir", HTTP_POST, [](AsyncWebServerRequest* r) { if (!checkAuth(r)) return; String e; if (fsMkdir(P(r, "path"), e)) ok(r); else ko(r, e); });
  server.on("/api/fs/del", HTTP_POST, [](AsyncWebServerRequest* r) { if (!checkAuth(r)) return; String e; if (fsRemove(P(r, "path"), e)) ok(r); else ko(r, e); });
  server.on("/api/fs/ren", HTTP_POST, [](AsyncWebServerRequest* r) { if (!checkAuth(r)) return; String e; if (fsRename(P(r, "from"), P(r, "to"), e)) ok(r); else ko(r, e); });
  server.on("/api/fs/save", HTTP_POST, [](AsyncWebServerRequest* r) { if (!checkAuth(r)) return; String e; if (fsWriteText(P(r, "path"), P(r, "text"), e, P(r, "new") == "1")) ok(r); else ko(r, e); });
  server.on("/api/fs/copy", HTTP_POST, [](AsyncWebServerRequest* r) { if (!checkAuth(r)) return; String e; if (fsCopy(P(r, "from"), P(r, "to"), e)) ok(r); else ko(r, e); });
  server.on("/api/fs/dirs", HTTP_GET, [](AsyncWebServerRequest* r) { if (!checkAuth(r)) return; sendJson(r, fsDirsJson()); });
  server.on("/api/fs/up", HTTP_POST,
    [](AsyncWebServerRequest* r) {
      if (!authSessionValid(token(r))) { r->send(401, "application/json", "{\"ok\":false,\"err\":\"non autorizzato\"}"); return; }
      if (g_upOk) ok(r); else ko(r, g_upErr.length() ? g_upErr : String(tr("Caricamento non riuscito")));
    },
    [](AsyncWebServerRequest* r, String filename, size_t index, uint8_t* data, size_t len, bool final) {
      if (!authSessionValid(token(r))) return;
      if (index == 0) {
        g_upOk = false; g_upErr = "";
        String dir = r->hasParam("dir") ? r->getParam("dir")->value() : String("/");
        String name = filename; int sl = name.lastIndexOf('/'); if (sl >= 0) name = name.substring(sl + 1);
        String p = fsClean((dir == "/" ? String("") : dir) + "/" + name);
        if (p.length() == 0 || fsProtected(p)) { g_upErr = tr("Nome o percorso non valido"); return; }
        if (g_upFile) g_upFile.close();
        { int ls = p.lastIndexOf('/'); if (ls > 0) { String d = p.substring(0, ls); if (!LittleFS.exists(d)) LittleFS.mkdir(d); } }
        g_upFile = LittleFS.open(p, "w");
        if (!g_upFile) { g_upErr = tr("Impossibile creare il file"); return; }
        g_upPath = p;
      }
      if (g_upFile) {
        if (g_upFile.write(data, len) != len) { g_upErr = tr("Spazio esaurito"); g_upFile.close(); LittleFS.remove(g_upPath); return; }
        if (final) { g_upFile.close(); g_upOk = true; }
      }
    });

  // Portale automatico: in modo AP ogni indirizzo sconosciuto (anche i controlli di Android, iPhone,
  // Windows: generate_204, hotspot-detect.html, connecttest.txt...) porta alla pagina della scheda.
  server.onNotFound([](AsyncWebServerRequest* r) {
    if (netCaptive() && !r->url().startsWith("/api/")) { r->redirect("http://192.168.4.1/"); return; }
    r->send(404, "text/plain", tr("Non trovato"));
  });
  vlog("WEB: percorsi pronti");
}

// Avvia il server solo quando la rete e gia inizializzata (chiamata dal task di rete)
void webStart() {
  static bool started = false;
  if (started) return;
  started = true;
  server.begin();
  vlog("WEB: server avviato sulla porta 80");
}
