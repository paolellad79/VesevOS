// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_web_net.cpp
// Percorsi del server web: configurazione, filtro IP, watchdog, servizi di rete, HTTPS. Aiuti e tipi condivisi: vos_web_int.h.
#include "vos_web_int.h"
#include "../core/vos_apwin.h"
#include "../core/vos_parse.h"
#include "../core/vos_clamp.h"
#include "../core/vos_wdset.h"
#include "../core/vos_ramwd_rule.h"
#include "../net/vos_net_ip6.h"

// stato del WPS per la pagina
static esp_err_t wpsReply(Res* s) {
  HeapBuf j(WPS_JSON_MAX);
  if (!wpsJson(j)) return ko(s, tr("Memoria insufficiente"), "off");
  return sendJson(s, j);
}

#if VOS_WITH_ELF
// Interruttore dei moduli ELF (servizio "elf", registro dei servizi). Nessuna conferma: yes=1, se arriva, e ignorato. Solo Admin (la rotta lo e gia).
static esp_err_t elfSwitch(Req* r, Res* s, Ctx& c) {
  if (!serviceFind("elf")) return ko(s, tr("Servizio sconosciuto"));
  bool on = Pis(r, "elf", "1");
  ErrBuf e;
  if (on && serviceState("elf") != SVC_ON) {
    if (!serviceStart("elf", e)) return ko(s, e);
  } else if (!on) {
    serviceStop("elf", e);
  }
  return sendJsonBuf(s, svcJson(c.role));
}
#endif

void routesNet(PsychicHttpServer* S, bool sec) {
  // ---- prima configurazione ----
  add(S, sec, "/api/setup/done", HTTP_POST, L_ADMIN, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    if (has(r, "ntp")) { cfg.ntpOn = Pis(r, "ntp", "1"); cfg.ntpAutoOff = !cfg.ntpOn; timeApply(); }     // senza Wi-Fi di casa: server dell'ora spento
    cfgEnsureApPass();                              // l'hotspot, quando c'e, ha sempre la sua password
    if (cfgSetupServices()) vlog("SETUP: Wi-Fi di casa configurata, hotspot e portale automatico restano spenti");   // l'utente li accende da Servizi
    // Ordine voluto: 1) la risposta parte e finisce; 2) il ritardo di 4 s comincia da qui (apWinClose); 3) solo dopo si scrive
    // cfg.setupDone. Se setupDone fosse scritto prima, il task di rete vedrebbe la guida finita e cambierebbe hotspot, DNS e HTTP
    // mentre la risposta e ancora in viaggio (prova 1.8.23e: risposta persa).
    esp_err_t sent = ok(s);
    apWinClose();                                   // l'hotspot passa a protetto 4 secondi dopo (dal task di rete)
    cfg.setupDone = true; cfgSave(); ledSetSetup(false);
    vlog("SETUP: prima configurazione completata");
    return sent;
  });

  // ---- controllo della configurazione (allarmi) e registro delle modifiche ----
  add(S, sec, "/api/audit", HTTP_GET, L_OPER, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    HeapBuf j(AUDIT_JSON_MAX);
    auditJson(j);
    if (j.oom() || j.over()) return ko(s, tr("Memoria insufficiente"), "off");
    return sendJson(s, j);
  });
  add(S, sec, "/api/audit/ack", HTTP_POST, L_OPER, [](Req* r, Res* s, Ctx& c) -> esp_err_t { auditAck(Pint(r, "n")); return ok(s); });

  // ---- filtro IP ----
  add(S, sec, "/api/fw", HTTP_GET, L_ADMIN, [](Req* r, Res* s, Ctx& c) -> esp_err_t { HeapBuf j(FW_JSON_MAX); return fwJson(j, c.ip) ? sendJson(s, j) : ko(s, tr("Memoria insufficiente")); });
  add(S, sec, "/api/fw", HTTP_POST, L_ADMIN, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    int mode = Pint(r, "mode");
    if (mode < 0 || mode > 3) return ko(s, tr("Modo non valido"));
    // voci: una per riga "indirizzo|1|nome" (indirizzo, intervallo a-b o rete/nn)
    FwRule nr[VOS_FW_MAX]; int n = 0;
    std::string_view rest(Pc(r, "rules"));
    int ln = 0;
    while (!rest.empty()) {
      size_t e = rest.find('\n');
      std::string_view l = trimView(rest.substr(0, e));
      rest = e == std::string_view::npos ? std::string_view() : rest.substr(e + 1);
      ln++;
      if (l.empty()) continue;
      if (n >= VOS_FW_MAX) { ErrBuf e; e.setf(tr("Massimo %d voci"), VOS_FW_MAX); return ko(s, e); }
      const size_t none = std::string_view::npos;
      size_t a1 = l.find('|'), a2 = a1 != none ? l.find('|', a1 + 1) : none;
      std::string_view rng = a1 != none ? l.substr(0, a1) : l;
      if (!cfgParseRange(rng, nr[n].a, nr[n].b)) { ErrBuf e; e.setf(tr("Voce %d: indirizzo non valido"), ln); return ko(s, e); }
      nr[n].on = a1 == none || l.substr(a1 + 1, a2 != none ? a2 - a1 - 1 : none) != "0";
      if (a2 != none) fwRuleSetName(nr[n], l.data() + a2 + 1, l.size() - a2 - 1); else nr[n].name[0] = 0;
      n++;
    }
    fwTryStart(c.ip);                                   // la regola nuova vale 2 minuti se non confermata
    cfg.fwMode = mode; cfg.fwNtp = Pis(r, "ntp", "1"); cfg.fwN = n;
    for (int i = 0; i < n; i++) cfg.fw[i] = nr[i];
    cfgSave();
    HeapBuf j(FW_JSON_MAX);
    return fwJson(j, c.ip) ? sendJson(s, j) : ko(s, tr("Memoria insufficiente"));
  });
  add(S, sec, "/api/fw/confirm", HTTP_POST, L_ADMIN, [](Req* r, Res* s, Ctx& c) -> esp_err_t { fwConfirm(); return ok(s); });

  // ---- watchdog ----
  add(S, sec, "/api/wd", HTTP_GET, L_OPER, [](Req* r, Res* s, Ctx& c) -> esp_err_t { HeapBuf j(WD_JSON_MAX); return wdJson(j) ? sendJson(s, j) : ko(s, tr("Memoria insufficiente")); });
  add(S, sec, "/api/wd", HTTP_POST, L_ADMIN, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    // tempi dei watchdog: tutti controllati insieme PRIMA di cambiare qualcosa; fuori intervallo = rifiutato, nessun modo di spegnerli
    WdChange wc;
    ErrBuf werr;
    if (has(r, "defaults") && Pis(r, "defaults", "1")) {          // solo i tempi tornano di base; il resto del watchdog non cambia
      return wdDefaults(werr) ? ok(s) : ko(s, werr.c_str());
    }
    if (has(r, "twdt")) wc.twdt = (int)Pint(r, "twdt");
    for (int i = 0; i < WD_LIM_N; i++) {
      char key[16]; snprintf(key, sizeof key, "lim_%s", WD_LIMITS[i].name);
      if (has(r, key)) wc.lim[i] = (int)Pint(r, key);
    }
    if (!wdCheck(wc, werr)) return ko(s, werr.c_str());
    cfg.wdTask = Pis(r, "task", "1"); cfg.wdNet = Pis(r, "net", "1"); cfg.wdRam = Pis(r, "ram", "1");
    cfg.wdNetMin = vosClamp((int)Pint(r, "netmin"), 2, 1440);
    cfg.wdRamPerc = ramWdClampCrit((int)Pint(r, "ramperc"));
    cfg.wdUpDays = vosClamp((int)Pint(r, "updays"), 0, 365);
    cfg.wdAt = vosClamp((int)Pint(r, "at"), -1, 1439);
    cfg.wdDays = Pint(r, "days") & 0x7F;
    if (!wdApply(wc, werr)) { cfgSave(); return ko(s, werr.c_str()); }   // wdApply salva da solo; qui solo se rifiuta il Task watchdog
    return ok(s);
  });

  // ---- servizi di rete: punto di accesso, HTTP, HTTPS, porte ----
  add(S, sec, "/api/svc", HTTP_GET, L_GUEST, [](Req* r, Res* s, Ctx& c) -> esp_err_t { return sendJsonBuf(s, svcJson(c.role)); });
  add(S, sec, "/api/svc", HTTP_POST, L_ADMIN, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
#if VOS_WITH_ELF
    if (has(r, "elf")) return elfSwitch(r, s, c);                  // interruttore dei moduli ELF: nessun'altra voce in questa richiesta
#endif
    bool apOn = has(r, "apOn") ? Pis(r, "apOn", "1") : cfg.apOn;
    bool cap = has(r, "captive") ? Pis(r, "captive", "1") : cfg.apCaptive;
    bool dh = has(r, "dhcpOn") ? Pis(r, "dhcpOn", "1") : cfg.dhcpOn;
    if (has(r, "apOn") && !has(r, "dhcpOn")) { if (!apOn && cfg.apOn) dh = false; else if (apOn && !cfg.apOn) dh = true; }   // il DHCP segue il punto di accesso
    int lease = has(r, "lease") ? (int)Pint(r, "lease") : cfg.dhcpLease;
    bool md = has(r, "mdnsOn") ? Pis(r, "mdnsOn", "1") : cfg.mdnsOn;
    if (lease < 10 || lease > 1440) return ko(s, tr("Durata dell'indirizzo: da 10 a 1440 minuti"));
    if (!dh && cfg.dhcpOn && netState() != NET_CLIENT_OK) return ko(s, tr("Spegni il DHCP dell'hotspot solo quando la scheda e collegata alla Wi-Fi di casa: senza DHCP il telefono dovrebbe avere un indirizzo fisso"), "state");
    bool hOn = has(r, "httpOn") ? Pis(r, "httpOn", "1") : cfg.httpOn;
    bool sOn = has(r, "httpsOn") ? Pis(r, "httpsOn", "1") : cfg.https;
    int hp = has(r, "httpPort") ? (int)Pint(r, "httpPort") : cfg.httpPort;
    int sp = has(r, "httpsPort") ? (int)Pint(r, "httpsPort") : cfg.httpsPort;
    const char* why = cfgSvcCheck(apOn, hOn, hp, sOn, sp);
    if (why) return ko(s, why);
    if (!apOn && cfg.apOn && netState() != NET_CLIENT_OK) return ko(s, tr("Spegni il Punto di accesso solo quando la scheda e collegata alla Wi-Fi di casa"), "state");
    g_cleanArm = false;                                  // modifica a mano: la pulizia automatica non serve piu
    cfg.apOn = apOn; cfg.apCaptive = cap; cfg.httpOn = hOn; cfg.https = sOn; cfg.httpPort = hp; cfg.httpsPort = sp;
    cfg.dhcpOn = dh; cfg.dhcpLease = lease; cfg.mdnsOn = md;
    cfgSave(); netApplyServices();
    return sendJsonBuf(s, svcJson(c.role));
  });

  // ---- WPS: collegamento alla Wi-Fi di casa con il router (pulsante o PIN) ----
  add(S, sec, "/api/wps", HTTP_GET, L_ADMIN, [](Req* r, Res* s, Ctx& c) -> esp_err_t { return wpsReply(s); });
  add(S, sec, "/api/wps", HTTP_POST, L_ADMIN, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    ErrBuf e;
    if (has(r, "on") && !wpsSetOn(Pis(r, "on", "1"), e)) return ko(s, e);
    if (has(r, "pinOk")) {
      cfg.wpsPin = Pis(r, "pinOk", "1");
      cfgSetOriginf("web admin %s", IpText(c.ip).s); cfgSave();
    }
    const char* go = Pc(r, "start");
    char who[24]; snprintf(who, sizeof who, "web %s", IpText(c.ip).s);
    bool pin = strcmp(go, "pin") == 0;
    if (pin || strcmp(go, "pbc") == 0) { if (!wpsStart(pin, who, e)) return ko(s, e, "state"); }
    else if (Pis(r, "stop", "1")) wpsStop();
    return wpsReply(s);
  });

  // ---- IPv6 della Wi-Fi di casa: lettura, interruttore e un indirizzo fisso in piu (gateway e DNS arrivano dal router) ----
  add(S, sec, "/api/ip6", HTTP_GET, L_OPER, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    HeapBuf j(NET6_JSON_MAX);
    if (!net6Json(j, netState() == NET_CLIENT_OK)) return ko(s, tr("Memoria insufficiente"), "off");
    return sendJson(s, j);
  });
  add(S, sec, "/api/ip6", HTTP_POST, L_ADMIN, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    ErrBuf e;
    bool acceso = has(r, "acceso") ? Pis(r, "acceso", "1") : cfg.ip6On;
    bool fisso = has(r, "fisso") ? Pis(r, "fisso", "1") : cfg.ip6Fisso;
    const char* ind = has(r, "ind") ? Pc(r, "ind") : cfg.ip6Ind.c_str();
    const char* gw = has(r, "gw") ? Pc(r, "gw") : cfg.ip6Gw.c_str();
    const char* dns = has(r, "dns") ? Pc(r, "dns") : cfg.ip6Dns.c_str();
    if (strlen(ind) > 45) return ko(s, tr("Indirizzo IPv6 non valido"));
    if (strlen(gw) > 45) return ko(s, tr("Gateway IPv6 non valido"));
    if (strlen(dns) > 45) return ko(s, tr("DNS IPv6 non valido"));
    cfgSetOriginf("web admin %s", IpText(c.ip).s);
    if (!net6Salva(acceso, fisso, ind, gw, dns, e)) return ko(s, e);
    HeapBuf j(NET6_JSON_MAX);
    if (!net6Json(j, netState() == NET_CLIENT_OK)) return ko(s, tr("Memoria insufficiente"), "off");
    return sendJson(s, j);
  });

  // ---- HTTPS ----
  add(S, sec, "/api/tls", HTTP_GET, L_GUEST, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    HeapBuf j(TLS_JSON_MAX);
    tlsJson(j);
    return sendJson(s, j);
  });
  add(S, sec, "/api/tls/cert", HTTP_GET, L_GUEST, [](Req* r, Res* s, Ctx& c) -> esp_err_t {      // il certificato e pubblico: serve per "fidarsi" nel browser
    if (!*tlsCertPem()) return ko(s, tr("Certificato non pronto"), "state");
    s->addHeader("Content-Disposition", "attachment; filename=\"vesevos.crt\"");
    return sendText(s, 200, "application/x-pem-file", tlsCertPem(), strlen(tlsCertPem()));
  });
  add(S, sec, "/api/tls", HTTP_POST, L_ADMIN, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    if (has(r, "on")) {
      bool on = Pis(r, "on", "1");
      const char* why = cfgSvcCheck(cfg.apOn, cfg.httpOn, cfg.httpPort, on, cfg.httpsPort);
      if (why) return ko(s, why);
      cfg.https = on; cfgSave();
    }
    if (Pis(r, "regen", "1")) tlsRegenerate();
    if (*Pc(r, "cert")) { ErrBuf e; if (!tlsSetCustom(Pc(r, "cert"), Pc(r, "key"), e)) return ko(s, e); }
    HeapBuf j(TLS_JSON_MAX);
    tlsJson(j);
    return sendJson(s, j);
  });
}
