// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_web_sys.cpp
// Percorsi del server web: MQTT, autodiagnosi, ora/NTP, localizzazione. Aiuti e tipi condivisi: vos_web_int.h.
#include "vos_web_int.h"
#include "../core/vos_cpupm.h"
#include "../devices/vos_sd.h"
#include "../core/vos_clamp.h"

// statistiche per la pagina
static esp_err_t statsReply(Res* s) {
  HeapBuf j(STATS_JSON_MAX);
  if (!statsJson(j)) return ko(s, tr("Memoria insufficiente"), "off");
  return sendJson(s, j);
}

void routesSys(PsychicHttpServer* S, bool sec) {
  // ---- MQTT (servizio spento di fabbrica: la prima volta si avvia a mano) ----
  add(S, sec, "/api/home", HTTP_GET, L_GUEST, [](Req* r, Res* s, Ctx& c) -> esp_err_t {      // un solo pezzo per i widget della Home
    HeapBuf j(1024);
    char t[320];
    snprintf(t, sizeof t, "{\"mesh\":%d,\"role\":%d,\"nodes\":%d,\"ntp\":%d,\"last\":%lu,\"ble\":%d,\"bleLeft\":%lu,\"bleLim\":%d,\"bleTot\":%lu,\"pw\":%d,\"stat\":%d",
             meshRunning() ? 1 : 0, (int)cfg.meshRole, (int)meshNodeCount(), cfg.ntpOn ? 1 : 0, (unsigned long)timeLastSync(),
             bleRunning() ? 1 : 0, (unsigned long)bleLeftSec(), bleLimited() ? 1 : 0, (unsigned long)bleTotalSec(), (int)cfg.pwMode, cfg.statOn ? 1 : 0);
    j.add(t);
    timePubJson(j);                                                                           // pezzo JSON gia pronto dall'ora (comincia con la virgola)
#if VOS_WITH_SD
    if (c.role >= ROLE_OPER) {                                                              // widget della microSD: solo se configurata, e non per gli ospiti
      char sd[SD_HOME_MAX]; sdHomeJson(sd, sizeof sd);
      if (sd[0]) { j.add(",\"sd\":"); j.add(sd); }
    }
#endif
    j.add('}');
    if (j.oom() || j.over()) return ko(s, tr("Memoria insufficiente"), "off");
    return sendJson(s, j);
  });
  add(S, sec, "/api/mqtt", HTTP_GET, L_OPER, [](Req* r, Res* s, Ctx& c) -> esp_err_t { 
    HeapBuf j(MQTT_JSON_MAX);
    if (!mqttStatusJson(j)) return sendJson(s, "{\"error\":\"busy\"}");   // come prima: mai JSON a meta
    return sendJson(s, j); });
  add(S, sec, "/api/mqtt", HTTP_POST, L_ADMIN, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    std::string_view host = Pt(r, "host"), pre = Pt(r, "prefix"), user = Pt(r, "user");
    long port = Pint(r, "port"), ev = Pint(r, "every");
    if (host.size() > 80 || user.size() > 60 || pre.size() > 60 || strlen(Pc(r, "pass")) > cfg.mqttPass.capacity()) return ko(s, tr("Valori troppo lunghi"), "toobig");
    for (char ch : host) { if (!(isalnum((unsigned char)ch) || ch == '.' || ch == '-')) return ko(s, tr("Broker: solo lettere, numeri, punto e trattino")); }
    for (char ch : pre) { if (ch == '#' || ch == '+' || ch < 33 || ch > 126) return ko(s, tr("Prefisso: niente spazi, # o +")); }
    if (port < 1 || port > 65535 || ev < 5 || ev > 3600) return ko(s, tr("Valori non validi"));
    cfg.mqttHost = host; cfg.mqttPort = port; cfg.mqttUser = user; cfg.mqttPrefix = pre; cfg.mqttEvery = ev;
    if (has(r, "pass") && *Pc(r, "pass")) cfg.mqttPass = Pc(r, "pass");
    if (Pis(r, "clearpass", "1")) cfg.mqttPass = "";
    cfg.mqttAuto = Pis(r, "auto", "1"); cfg.mqttHa = Pis(r, "ha", "1"); cfg.mqttTls = Pis(r, "tls", "1");
    cfgSave();
    if (mqttRunning()) { ErrBuf e; serviceRestart("mqtt", e); }      // riparte con i valori nuovi
    return ok(s);
  });
  add(S, sec, "/api/mqtt/ca", HTTP_POST, L_ADMIN, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    std::string_view pem(Pc(r, "pem")); ErrBuf e;
    if (pem.empty()) { drvFsRemove("/mqtt-ca.pem"); return ok(s); }
    if (pem.find("-----BEGIN CERTIFICATE-----") == std::string_view::npos || pem.size() > 8000) return ko(s, tr("Certificato non valido (formato PEM)"));
    return fsWriteText("/mqtt-ca.pem", pem, e) ? ok(s) : ko(s, e);
  });
  add(S, sec, "/api/mqtt/run", HTTP_POST, L_OPER, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    ErrBuf e;
    if (Pis(r, "a", "start")) { if (!serviceStart("mqtt", e)) return ko(s, e); }
    else if (Pis(r, "a", "restart")) { if (!serviceRestart("mqtt", e)) return ko(s, e); }
    else if (Pis(r, "a", "stop")) serviceStop("mqtt", e);
    else if (Pis(r, "a", "test")) { if (!mqttPublishRel("test", "VesevOS " VOS_VERSION)) return ko(s, tr("MQTT non collegato"), "state"); }
    else return ko(s, tr("Azione non valida"));
    return ok(s);
  });
  add(S, sec, "/api/reboot", HTTP_POST, L_OPER, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    char w[96]; whoTo(c, w, sizeof w);
    vlog("SISTEMA: riavvio dalla pagina (%s)", w);
    esp_err_t e = ok(s);
    xTaskCreate(delayedRestart, "rst", 2048, (void*)"dalla pagina", 1, NULL);
    return e;
  });
  add(S, sec, "/api/crash", HTTP_GET, L_OPER, [](Req* r, Res* s, Ctx& c) -> esp_err_t { std::string t = crashText(); return sendText(s, 200, "text/plain; charset=utf-8", t.c_str(), t.size()); });   // rapporti dei crash
  add(S, sec, "/api/who", HTTP_GET, L_ADMIN, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    HeapBuf j(AUTH_TEXT_MAX);
    authWhoJson(j);
    if (j.oom() || j.over()) return ko(s, tr("Memoria insufficiente"), "off");
    return sendJson(s, j);
  });   // chi e collegato e cosa fa
  add(S, sec, "/api/recovery", HTTP_GET, L_ADMIN, [](Req* r, Res* s, Ctx& c) -> esp_err_t {      // il recovery c'e? (aggiornamento del firmware)
    bool pr = drvRecoveryPresent();
    HeapBuf j(160);
    char t[64]; snprintf(t, sizeof t, "{\"present\":%d,\"kb\":%lu,\"ver\":\"", pr ? 1 : 0, (unsigned long)drvRecoverySizeKB());
    j.add(t);
    if (pr) { j.addJson(recVersion()); }
    j.add("\"}");
    return sendJson(s, j);
  });
  add(S, sec, "/api/recovery", HTTP_POST, L_ADMIN, [](Req* r, Res* s, Ctx& c) -> esp_err_t {  // riavvia nel recovery (pagina di caricamento del .bin)
    if (!drvRecoveryPresent()) return ko(s, tr("Recovery non presente: la scheda ha ancora la tabella partizioni vecchia."), "state");
    if (!drvRecoveryEnter()) return ko(s, tr("Non riesco a passare al recovery."), "state");
    char w[96]; whoTo(c, w, sizeof w);
    vlog("SISTEMA: riavvio nel recovery dalla pagina (%s)", w);
    char ip[16], body[48]; netIpTo(ip, sizeof ip);
    snprintf(body, sizeof body, "{\"ok\":true,\"ip\":\"%s\"}", ip);
    esp_err_t e = sendJson(s, body);
    xTaskCreate(delayedRestart, "rst", 2048, (void*)"recovery dalla pagina", 1, NULL);
    return e;
  });
  // aggiornamento del recovery: il file va in PSRAM, si controlla tutto e solo dopo si scrive (vos_recupd). Accesso controllato anche durante l'invio.
  static ErrBuf s_recErr;
  PsychicUploadHandler* ru = new PsychicUploadHandler();
  ru->onUpload([sec](Req* r, const auto& filename, uint64_t index, uint8_t* data, size_t len, bool final) -> esp_err_t {
    uint32_t ip = ipOf(r);
    if (!fwAllow(ip, false)) return ESP_FAIL;
    int u = userOf(r);
    if (u < 0 || cfg.users[u].role < ROLE_ADMIN) return ESP_FAIL;
    if (!sec && g_tlsUp && cfg.https && !netOnAp(ip)) return ESP_FAIL;
    if (index == 0) {
      s_recErr.clear();
      if (r->contentLength() > drvRecoverySize() + 4096) { s_recErr = tr("File troppo grande per lo spazio del recovery"); return ESP_OK; }   // si capisce subito (es. il firmware)
      ErrBuf e; if (!recUpBegin(e)) { s_recErr = e.c_str(); return ESP_OK; }
    }
    if (!s_recErr.empty()) return ESP_OK;
    recUpAdd(data, len);                                            // se e troppo grande lo dice recUpFinish
    return ESP_OK;
  });
  ru->onRequest([](Req* r, Res* s) -> esp_err_t {
    int u = userOf(r);
    if (u < 0 || cfg.users[u].role < ROLE_ADMIN) { recUpAbort(); return sendJson(s, "{\"ok\":false,\"err\":\"non autorizzato\"}", 401); }
    if (!s_recErr.empty()) return ko(s, s_recErr);
    char from[16], to[16], w[64], sha[80]; ErrBuf e;
    snprintf(w, sizeof w, "web %s %s", cfg.users[u].name.c_str(), IpText(ipOf(r)).s);
    qparamTo(r, "sha", sha, sizeof sha);
    if (!recUpFinish(sha, w, from, to, sizeof from, e)) return ko(s, e);
    HeapBuf j(128);
    j.add("{\"ok\":true,\"from\":\""); j.addJson(from, strlen(from));
    j.add("\",\"to\":\""); j.addJson(to, strlen(to)); j.add("\"}");
    return sendJson(s, j);
  });
  S->on("/api/recovery/up", HTTP_POST, ru);
  add(S, sec, "/api/sleep", HTTP_POST, L_ADMIN, [](Req* r, Res* s, Ctx& c) -> esp_err_t {   // solo Admin (decisione del 23/10/2026: l'Operatore non addormenta la scheda)
    g_sleepSec = has(r, "min") ? (uint32_t)(Pint(r, "min") * 60L) : 0;     // 0 = fino a RESET
    if (g_sleepSec > 7UL * 24 * 3600) return ko(s, tr("Sonno: da 1 minuto a 7 giorni"));
    esp_err_t e = ok(s);
    xTaskCreate(delayedSleep, "slp", 3072, NULL, 1, NULL);
    return e;
  });
  // ---- autodiagnosi: prove una per volta + report di testo (solo Admin) ----
  add(S, sec, "/api/selftest", HTTP_GET, L_ADMIN, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    HeapBuf j(SELFTEST_JSON_MAX);
    if (!selftestJson(j)) return ko(s, tr("Memoria insufficiente"), "off");
    return sendJson(s, j);
  });
  add(S, sec, "/api/selftest", HTTP_POST, L_ADMIN, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    ErrBuf e;
    if (Pis(r, "clear", "1")) { selftestClear(); return ok(s); }
    if (!selftestStart(Pis(r, "active", "1"), Pis(r, "names", "1"), e)) return ko(s, e);
    return ok(s);
  });
  add(S, sec, "/api/selftest/report", HTTP_GET, L_ADMIN, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    HeapBuf rp(SELFTEST_REPORT_MAX);
    if (!selftestReport(rp)) return (rp.oom() || rp.over()) ? ko(s, tr("Memoria insufficiente"), "off") : ko(s, tr("Nessun report: lancia prima la prova"), "notfound");
    s->addHeader("Content-Disposition", "attachment; filename=\"vesevos-report.txt\"");
    return sendText(s, 200, "text/plain; charset=utf-8", rp.c_str(), rp.length());
  });
  add(S, sec, "/api/stats", HTTP_GET, L_OPER, [](Req* r, Res* s, Ctx& c) -> esp_err_t { return statsReply(s); });
  add(S, sec, "/api/stats.csv", HTTP_GET, L_OPER, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    s->addHeader("Content-Disposition", "attachment; filename=\"vesevos-statistiche.csv\"");
    std::string csv = statsCsv();
    return sendText(s, 200, "text/csv", csv.c_str(), csv.size());
  });
  add(S, sec, "/api/stats", HTTP_POST, L_ADMIN, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    if (has(r, "on")) { bool on = Pis(r, "on", "1"); if (on && !cfg.statOn) statsReset(); cfg.statOn = on; cfgSetOrigin("statistiche"); cfgSave(); }
    if (Pis(r, "reset", "1")) statsReset();
    return statsReply(s);
  });
  add(S, sec, "/api/power", HTTP_GET, L_OPER, [](Req* r, Res* s, Ctx& c) -> esp_err_t { HeapBuf j(POWER_JSON_MAX); return powerJson(j) ? sendJson(s, j) : ko(s, tr("Memoria insufficiente")); });
  add(S, sec, "/api/power", HTTP_POST, L_ADMIN, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    ErrBuf e;
    powerSet((int)Pint(r, "mode"), (uint32_t)Pint(r, "awake"), (uint32_t)Pint(r, "sleep"), e);
    if (e.length()) return ko(s, e);
    cfgSetOrigin("risparmio energia"); cfgSave();
    HeapBuf j(POWER_JSON_MAX);
    return powerJson(j) ? sendJson(s, j) : ko(s, tr("Memoria insufficiente"));
  });
  add(S, sec, "/api/factory", HTTP_POST, L_ADMIN, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    char w[96]; whoTo(c, w, sizeof w);
    vlog("SISTEMA: ripristino di fabbrica dalla pagina (%s)", w);
    cfgFactoryReset();
    esp_err_t e = ok(s);
    xTaskCreate(delayedRestart, "rst", 2048, (void*)"ripristino di fabbrica dalla pagina", 1, NULL);
    return e;
  });

  // ---- ora / NTP ----
  add(S, sec, "/api/time", HTTP_GET, L_GUEST, [](Req* r, Res* s, Ctx& c) -> esp_err_t { HeapBuf j(TIME_JSON_MAX); return timeJson(j) ? sendJson(s, j) : ko(s, tr("Memoria insufficiente")); });
  add(S, sec, "/api/time", HTTP_POST, L_ADMIN, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    std::string_view srv = Pt(r, "server"), srv2 = Pt(r, "server2");
    if (srv.size() < 1 || srv.size() > 60) return ko(s, tr("Server NTP non valido"));
    for (char ch : srv) { if (!(isalnum((unsigned char)ch) || ch == '.' || ch == '-')) return ko(s, tr("Server NTP: solo lettere, numeri, punto e trattino")); }
    if (has(r, "server2") && !srv2.empty() && !utilHostOk(srv2)) return ko(s, tr("Server NTP: solo lettere, numeri, punto e trattino"));
    long ev = has(r, "every") ? Pint(r, "every") : (long)cfg.ntpEvery;
    if (!timeEveryValid(ev)) return ko(s, tr("Frequenza non valida"));
    cfg.ntpEvery = ev;
    cfg.ntpOn = Pis(r, "ntp", "1"); cfg.ntpAutoOff = false; cfg.ntpServe = Pis(r, "serve", "1");
    cfg.ntpServer = srv; if (has(r, "server2")) cfg.ntpServer2 = srv2;
    cfgSave(); timeApply(); return ok(s);
  });
  add(S, sec, "/api/time/sync", HTTP_POST, L_OPER, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    if (!cfg.ntpOn) return ko(s, tr("La sincronizzazione NTP e spenta"), "state");
    if (netState() != NET_CLIENT_OK) return ko(s, tr("Serve la connessione a una rete Wi-Fi"), "state");
    timeApply(); vlog("TIME: sincronizzazione richiesta dalla pagina"); return ok(s);
  });
  add(S, sec, "/api/time/set", HTTP_POST, L_ADMIN, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    if (has(r, "local")) { ErrBuf err; return timeSetLocal(Pc(r, "local"), err) ? ok(s) : ko(s, err); }
    unsigned long e = strtoul(Pc(r, "epoch"), NULL, 10);
    if (e < 1700000000UL) return ko(s, tr("Ora non valida"));
    timeSetEpoch(e); return ok(s);
  });
  add(S, sec, "/api/cpu", HTTP_POST, L_ADMIN, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    // Scelta: "gov" (80, 160, 240, ondemand, conservative, schedutil) oppure il vecchio "mode" (0, 80, 160, 240). Parametri: up, down, hold.
    std::string choice;                                                // la scelta, controllata PRIMA di cambiare qualsiasi cosa
    if (has(r, "gov")) choice = std::string(Pt(r, "gov"));
    else if (has(r, "mode")) {
      int m = (int)Pint(r, "mode");
      choice = m == 0 ? "ondemand" : std::to_string(m);
    }
    bool hasParam = has(r, "up") || has(r, "down") || has(r, "hold");
    if (choice.empty() && !hasParam) return ko(s, tr("Valore non valido"));
    if (!choice.empty() && !cpuChoiceValid(choice.c_str())) return ko(s, tr("Valore non valido"));
    if (hasParam) {                                                     // quelli non indicati restano come sono
      ErrBuf err;
      int up = has(r, "up") ? (int)Pint(r, "up") : (int)cfg.cpuUp;
      int down = has(r, "down") ? (int)Pint(r, "down") : (int)cfg.cpuDown;
      int hold = has(r, "hold") ? (int)Pint(r, "hold") : (int)cfg.cpuHold;
      if (!cpuParamsSet(up, down, hold, err)) return ko(s, err);
    }
    if (!choice.empty()) cpuChoiceSet(choice.c_str());
    return ok(s);
  });

  // ---- localizzazione: paese, fuso, formati, unita, radio ----
  add(S, sec, "/api/region", HTTP_GET, L_GUEST, [](Req* r, Res* s, Ctx& c) -> esp_err_t { HeapBuf j(REGION_JSON_MAX); return regionJson(j) ? sendJson(s, j) : ko(s, tr("Memoria insufficiente")); });
  add(S, sec, "/api/region", HTTP_POST, L_ADMIN, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    bool radio = false;
    if (has(r, "country")) {
      char cc[8];                                                       // il codice paese ha 2 lettere: oltre la capacita non puo essere valido
      std::string_view v = Pt(r, "country");
      if (v.size() >= sizeof cc) return ko(s, tr("Paese sconosciuto"));
      for (size_t i = 0; i < v.size(); i++) cc[i] = (char)toupper((unsigned char)v[i]);
      cc[v.size()] = 0;
      if (cc[0] && !regionValid(cc)) return ko(s, tr("Paese sconosciuto"));
      if (strcmp(cc, cfg.country.c_str()) != 0) { cfg.country = cc; radio = true; }
    }
    if (has(r, "tz")) {
      const char* tz = Pc(r, "tz"); const char* tzn = Pc(r, "tzname");
      size_t tl = strlen(tz);
      if (tl < 3 || tl > 60) return ko(s, tr("Fuso orario non valido"));
      for (size_t i = 0; i < tl; i++) { unsigned char ch = tz[i]; if (ch < 32 || ch >= 127 || ch == '\'' || ch == '"') return ko(s, tr("Fuso orario: caratteri non validi")); }
      cfg.tz = tz; if (*tzn) { char nm[64]; cleanAscii(tzn, nm, sizeof nm); cfg.tzName = nm; } else cfg.tzName = "Personalizzato";
      setenv("TZ", cfg.tz.c_str(), 1); tzset();
    }
    if (has(r, "ntp")) { std::string_view srv = Pt(r, "ntp"); if (srv.size() >= 1 && srv.size() <= 60) cfg.ntpServer = srv; }
    if (has(r, "datefmt")) cfg.dateFmt = vosClamp((int)Pint(r, "datefmt"), 0, 2);
    if (has(r, "timefmt")) cfg.timeFmt = vosClamp((int)Pint(r, "timefmt"), 0, 1);
    if (has(r, "tempunit")) cfg.tempUnit = vosClamp((int)Pint(r, "tempunit"), 0, 1);
    if (has(r, "weekstart")) cfg.weekStart = Pis(r, "weekstart", "1") ? 1 : 0;
    if (has(r, "decsep")) cfg.decSep = Pis(r, "decsep", "1") ? 1 : 0;
    if (has(r, "antenna")) { uint8_t a = Pis(r, "antenna", "1") ? 1 : 0; if (a != cfg.antExt) radio = true; cfg.antExt = a; }
    if (has(r, "gain")) { int g = (int)Pint(r, "gain"); if (g < 0 || g > 15) return ko(s, tr("Guadagno dell'antenna: da 0 a 15 dBi")); if (g != cfg.antGain) radio = true; cfg.antGain = g; }
    if (has(r, "txpower")) { int t = (int)Pint(r, "txpower"); if (t < 0 || t > 20) return ko(s, tr("Potenza non valida")); if (t != cfg.txDbm) radio = true; cfg.txDbm = t; }
    cfgSave(); timeApply();
    if (radio && !cfg.airOn) regionApplyRadio();
    HeapBuf j(REGION_JSON_MAX);
    return regionJson(j) ? sendJson(s, j) : ko(s, tr("Memoria insufficiente"));
  });
}
