// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_web_sys.cpp
// Percorsi del server web: MQTT, autodiagnosi, ora/NTP, localizzazione. Aiuti e tipi condivisi: vos_web_int.h.
#include "vos_web_int.h"

void routesSys(PsychicHttpServer* S, bool sec) {
  // ---- MQTT (servizio spento di fabbrica: la prima volta si avvia a mano) ----
  add(S, sec, "/api/home", HTTP_GET, L_GUEST, [](Req* r, Res* s, Ctx& c) -> esp_err_t {      // un solo pezzo per i widget della Home
    String j = "{\"mesh\":" + String(meshRunning() ? 1 : 0) + ",\"role\":" + String((int)cfg.meshRole) + ",\"nodes\":" + String(meshNodeCount()) +
               ",\"ntp\":" + String(cfg.ntpOn ? 1 : 0) + ",\"last\":" + String((unsigned long)timeLastSync()) +
               ",\"ble\":" + String(bleRunning() ? 1 : 0) + ",\"bleLeft\":" + String((unsigned long)bleLeftSec()) + ",\"bleLim\":" + String(bleLimited() ? 1 : 0) + ",\"bleTot\":" + String((unsigned long)bleTotalSec()) +
               ",\"pw\":" + String((int)cfg.pwMode) + ",\"stat\":" + String(cfg.statOn ? 1 : 0) + timePubJson() + "}";
    return sendJson(s, j);
  });
  add(S, sec, "/api/mqtt", HTTP_GET, L_OPER, [](Req* r, Res* s, Ctx& c) -> esp_err_t { return sendJson(s, mqttStatusJson()); });
  add(S, sec, "/api/mqtt", HTTP_POST, L_ADMIN, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    String host = P(r, "host"), pre = P(r, "prefix"), user = P(r, "user");
    long port = P(r, "port").toInt(), ev = P(r, "every").toInt();
    host.trim(); pre.trim(); user.trim();
    if (host.length() > 80 || user.length() > 60 || pre.length() > 60) return ko(s, tr("Valori troppo lunghi"), "toobig");
    for (size_t i = 0; i < host.length(); i++) { char ch = host[i]; if (!(isAlphaNumeric(ch) || ch == '.' || ch == '-')) return ko(s, tr("Broker: solo lettere, numeri, punto e trattino")); }
    for (size_t i = 0; i < pre.length(); i++) { char ch = pre[i]; if (ch == '#' || ch == '+' || ch < 33 || ch > 126) return ko(s, tr("Prefisso: niente spazi, # o +")); }
    if (port < 1 || port > 65535 || ev < 5 || ev > 3600) return ko(s, tr("Valori non validi"));
    cfg.mqttHost = host; cfg.mqttPort = port; cfg.mqttUser = user; cfg.mqttPrefix = pre; cfg.mqttEvery = ev;
    if (has(r, "pass") && P(r, "pass").length()) cfg.mqttPass = P(r, "pass");
    if (P(r, "clearpass") == "1") cfg.mqttPass = "";
    cfg.mqttAuto = P(r, "auto") == "1"; cfg.mqttHa = P(r, "ha") == "1"; cfg.mqttTls = P(r, "tls") == "1";
    cfgSave();
    if (mqttRunning()) { String e; serviceRestart("mqtt", e); }      // riparte con i valori nuovi
    return ok(s);
  });
  add(S, sec, "/api/mqtt/ca", HTTP_POST, L_ADMIN, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    String pem = P(r, "pem"), e;
    if (!pem.length()) { drvFsRemove("/mqtt-ca.pem"); return ok(s); }
    if (pem.indexOf("-----BEGIN CERTIFICATE-----") < 0 || pem.length() > 8000) return ko(s, tr("Certificato non valido (formato PEM)"));
    return fsWriteText("/mqtt-ca.pem", pem, e) ? ok(s) : ko(s, e);
  });
  add(S, sec, "/api/mqtt/run", HTTP_POST, L_OPER, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    String a = P(r, "a");
    String e;
    if (a == "start") { if (!serviceStart("mqtt", e)) return ko(s, e); }
    else if (a == "restart") { if (!serviceRestart("mqtt", e)) return ko(s, e); }
    else if (a == "stop") serviceStop("mqtt", e);
    else if (a == "test") { if (!mqttPublishRel("test", "VesevOS " VOS_VERSION)) return ko(s, tr("MQTT non collegato"), "state"); }
    else return ko(s, tr("Azione non valida"));
    return ok(s);
  });
  add(S, sec, "/api/reboot", HTTP_POST, L_OPER, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    vlog("SISTEMA: riavvio dalla pagina (%s)", who(c).c_str());
    esp_err_t e = ok(s);
    xTaskCreate(delayedRestart, "rst", 2048, NULL, 1, NULL);
    return e;
  });
  add(S, sec, "/api/recovery", HTTP_GET, L_ADMIN, [](Req* r, Res* s, Ctx& c) -> esp_err_t {      // il recovery c'e? (aggiornamento del firmware)
    return sendJson(s, String("{\"present\":") + (drvRecoveryPresent() ? 1 : 0) + ",\"kb\":" + String((unsigned long)drvRecoverySizeKB()) + "}");
  });
  add(S, sec, "/api/recovery", HTTP_POST, L_ADMIN, [](Req* r, Res* s, Ctx& c) -> esp_err_t {  // riavvia nel recovery (pagina di caricamento del .bin)
    if (!drvRecoveryPresent()) return ko(s, tr("Recovery non presente: la scheda ha ancora la tabella partizioni vecchia."), "state");
    if (!drvRecoveryEnter()) return ko(s, tr("Non riesco a passare al recovery."), "state");
    vlog("SISTEMA: riavvio nel recovery dalla pagina (%s)", who(c).c_str());
    esp_err_t e = sendJson(s, "{\"ok\":true,\"ip\":\"" + netIpString() + "\"}");
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
    if (!rp.length()) return ko(s, tr("Nessun report: lancia prima la prova"), "notfound");
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
    String srv = P(r, "server"), srv2 = P(r, "server2"); srv.trim(); srv2.trim();
    if (srv.length() < 1 || srv.length() > 60) return ko(s, tr("Server NTP non valido"));
    for (size_t i = 0; i < srv.length(); i++) { char ch = srv[i]; if (!(isAlphaNumeric(ch) || ch == '.' || ch == '-')) return ko(s, tr("Server NTP: solo lettere, numeri, punto e trattino")); }
    if (has(r, "server2") && srv2.length() && !utilHostOk(srv2)) return ko(s, tr("Server NTP: solo lettere, numeri, punto e trattino"));
    long ev = has(r, "every") ? P(r, "every").toInt() : (long)cfg.ntpEvery;
    if (!timeEveryValid(ev)) return ko(s, tr("Frequenza non valida"));
    cfg.ntpEvery = ev;
    cfg.ntpOn = P(r, "ntp") == "1"; cfg.ntpAutoOff = false; cfg.ntpServe = P(r, "serve") == "1";
    cfg.ntpServer = srv; if (has(r, "server2")) cfg.ntpServer2 = srv2;
    cfgSave(); timeApply(); return ok(s);
  });
  add(S, sec, "/api/time/sync", HTTP_POST, L_OPER, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    if (!cfg.ntpOn) return ko(s, tr("La sincronizzazione NTP e spenta"), "state");
    if (netState() != NET_CLIENT_OK) return ko(s, tr("Serve la connessione a una rete Wi-Fi"), "state");
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
}
