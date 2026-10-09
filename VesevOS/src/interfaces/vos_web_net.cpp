// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_web_net.cpp
// Percorsi del server web: configurazione, filtro IP, watchdog, servizi di rete, HTTPS. Aiuti e tipi condivisi: vos_web_int.h.
#include "vos_web_int.h"

void routesNet(PsychicHttpServer* S, bool sec) {
  // ---- prima configurazione ----
  add(S, sec, "/api/setup/done", HTTP_POST, L_ADMIN, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    if (has(r, "ntp")) { cfg.ntpOn = P(r, "ntp") == "1"; cfg.ntpAutoOff = !cfg.ntpOn; timeApply(); }     // senza Wi-Fi di casa: server dell'ora spento
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
    if (has(r, "apOn") && !has(r, "dhcpOn")) { if (!apOn && cfg.apOn) dh = false; else if (apOn && !cfg.apOn) dh = true; }   // il DHCP segue il punto di accesso
    int lease = has(r, "lease") ? (int)P(r, "lease").toInt() : cfg.dhcpLease;
    bool md = has(r, "mdnsOn") ? P(r, "mdnsOn") == "1" : cfg.mdnsOn;
    if (lease < 10 || lease > 1440) return ko(s, tr("Durata dell'indirizzo: da 10 a 1440 minuti"));
    if (!dh && cfg.dhcpOn && netState() != NET_CLIENT_OK) return ko(s, tr("Spegni il DHCP dell'hotspot solo quando la scheda e collegata alla Wi-Fi di casa: senza DHCP il telefono dovrebbe avere un indirizzo fisso"), "state");
    bool hOn = has(r, "httpOn") ? P(r, "httpOn") == "1" : cfg.httpOn;
    bool sOn = has(r, "httpsOn") ? P(r, "httpsOn") == "1" : cfg.https;
    int hp = has(r, "httpPort") ? (int)P(r, "httpPort").toInt() : cfg.httpPort;
    int sp = has(r, "httpsPort") ? (int)P(r, "httpsPort").toInt() : cfg.httpsPort;
    String e = cfgSvcCheck(apOn, hOn, hp, sOn, sp);
    if (e.length()) return ko(s, e);
    if (!apOn && cfg.apOn && netState() != NET_CLIENT_OK) return ko(s, tr("Spegni il Punto di accesso solo quando la scheda e collegata alla Wi-Fi di casa"), "state");
    g_cleanArm = false;                                  // modifica a mano: la pulizia automatica non serve piu
    cfg.apOn = apOn; cfg.apCaptive = cap; cfg.httpOn = hOn; cfg.https = sOn; cfg.httpPort = hp; cfg.httpsPort = sp;
    cfg.dhcpOn = dh; cfg.dhcpLease = lease; cfg.mdnsOn = md;
    cfgSave(); netApplyServices();
    return sendJson(s, svcJson());
  });

  // ---- HTTPS ----
  add(S, sec, "/api/tls", HTTP_GET, L_GUEST, [](Req* r, Res* s, Ctx& c) -> esp_err_t { return sendJson(s, tlsJson()); });
  add(S, sec, "/api/tls/cert", HTTP_GET, L_GUEST, [](Req* r, Res* s, Ctx& c) -> esp_err_t {      // il certificato e pubblico: serve per "fidarsi" nel browser
    if (!tlsCertPem().length()) return ko(s, tr("Certificato non pronto"), "state");
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
}
