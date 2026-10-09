// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_web_dev.cpp
// Percorsi del server web: periferiche, pin, LED, automazioni. Aiuti e tipi condivisi: vos_web_int.h.
#include "vos_web_int.h"
#include "../core/vos_eventbus.h"

void routesDev(PsychicHttpServer* S, bool sec) {
  // ---- periferiche (virtuali e hardware): stesso modo di rispondere per tutte ----
  add(S, sec, "/api/dev", HTTP_GET, L_OPER, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    if (has(r, "id")) { String j = devStatusJson(P(r, "id"), c.role); return j.length() ? sendJson(s, j) : notFound(s); }
    return sendJson(s, devListJson());
  });
  add(S, sec, "/api/events", HTTP_GET, L_OPER, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    long n = has(r, "n") ? P(r, "n").toInt() : 20;
    return sendJson(s, EventBus::json((uint32_t)max(0L, P(r, "since").toInt()), (int)constrain(n, 1L, 40L)));
  });
  add(S, sec, "/api/dev/set", HTTP_POST, L_ADMIN, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    String e; return devSet(P(r, "id"), P(r, "on") == "1", e) ? ok(s) : ko(s, e);
  });
  add(S, sec, "/api/dev/act", HTTP_POST, L_OPER, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    String out, e;
    if (!devAct(P(r, "id"), P(r, "a"), P(r, "arg"), c.role, out, e)) return ko(s, e);
    return out.length() ? sendJson(s, out) : ok(s);
  });
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

  add(S, sec, "/api/serial", HTTP_GET, L_ADMIN, [](Req* r, Res* s, Ctx& c) -> esp_err_t { return sendJson(s, serialJson()); });
  add(S, sec, "/api/serial", HTTP_POST, L_ADMIN, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    String k = P(r, "k"), v = P(r, "v"), e;
    if (k == "keep") { serialKeep(); return sendJson(s, serialJson()); }
    if (!serialSet(k, v, e)) return ko(s, e);
    auditEvent(AUD_INFO, "", trf("seriale: %s = %s (%s)", k.c_str(), v.c_str(), who(c).c_str()));
    return sendJson(s, serialJson());
  });
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
    bool ntpBack = cfgNtpAfterWifi();
    cfgSave(); netReconfigure(); if (ntpBack) timeApply(); return ok(s);
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
  // esportazione con i segreti: solo Admin, solo dopo aver accettato (acc=1), sempre cifrata con una frase (mai in chiaro)
  add(S, sec, "/api/config/export", HTTP_POST, L_ADMIN, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    if (P(r, "acc") != "1") return ko(s, tr("Devi accettare l'avviso per esportare i segreti"));
    String pw = P(r, "pw"), out, err;
    if (!cryptSeal(cfgExport(true), pw, out, err)) return ko(s, err);
    vlog("CFG: esportazione con segreti (cifrata) da %s", who(c).c_str());
    auditEvent(AUD_YELLOW, "cfgexp", trf("Backup con segreti scaricato da %s", who(c).c_str()));
    s->addHeader("Content-Disposition", "attachment; filename=\"vesevos-segreti.conf\"");
    return sendText(s, 200, "text/plain; charset=utf-8", out);
  });
  add(S, sec, "/api/config/restore", HTTP_POST, L_ADMIN, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    String t = P(r, "t"), err;
    if (t.length() == 0 || t.length() > 46000) return ko(s, tr("File vuoto o troppo grande"), "toobig");
    if (cryptIsSealed(t)) {
      String plain;
      if (!P(r, "pw").length()) return ko(s, tr("Il file e cifrato: scrivi la frase"));
      if (!cryptOpen(t, P(r, "pw"), plain, err)) { authFailIp(c.ip, "frase del backup sbagliata"); return ko(s, err); }
      t = plain;
    }
    String before = cfgExport(true);
    if (!cfgImport(t, err)) return ko(s, err);
    cfgSave(); ledApplyConfig(); netReconfigure();
    String after = cfgExport(true); int ch = 0, i = 0;                   // righe cambiate rispetto a prima
    while (i < (int)after.length()) {
      int e = after.indexOf('\n', i); if (e < 0) e = after.length();
      String ln = after.substring(i, e); i = e + 1;
      if (ln.length() && before.indexOf(ln + "\n") < 0) ch++;
    }
    vlog("CFG: ripristino da file (%d righe cambiate) da %s", ch, who(c).c_str());
    return sendJson(s, "{\"ok\":true,\"changed\":" + String(ch) + "}");
  });

  add(S, sec, "/api/airplane", HTTP_POST, L_OPER, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    if (P(r, "on") != "1") { netAirplaneOff("dalla pagina"); return ok(s); }
    String err;
    return netAirplaneOn(P(r, "exit").toInt(), strtoul(P(r, "param").c_str(), NULL, 10), err) ? ok(s) : ko(s, err);
  });
}
