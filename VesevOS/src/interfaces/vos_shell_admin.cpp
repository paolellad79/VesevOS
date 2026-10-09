// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_shell_admin.cpp
// Comandi della shell: configurazione, utenti, sicurezza, rete schede, servizi, energia, riavvio. Aiuti e include: vos_shell_int.h.
#include "vos_shell_int.h"

bool shCmdAdmin(String c, String a1, const String& line, Print& o, int role) {
  if (c == "config") o.print(cfgExport(false));
  else if (c == "passwd") {
    String np = restFrom(line, 2), err;
    int u = userFind(a1);
    if (u < 0 || !np.length()) { o.println(tr("Uso: passwd <utente> <nuova password>")); return true; }
    if (!userSetPassword(u, np, err)) { o.println(err); return true; }
    cfgSave(); serialAuthSet(true);
    o.println(tr("Password cambiata"));
  }
  else if (c == "user" || c == "users") {
    String a2 = argAt(line, 2), a3 = argAt(line, 3), err;
    if (a1 == "" || a1 == "list") { o.print(usersText()); return true; }
    if (a1 == "add") { if (!userAdd(a2, roleArg(a3) < 0 ? 255 : roleArg(a3), restFrom(line, 4), err)) { o.println(err); return true; } }
    else if (a1 == "del") { if (!userDel(userFind(a2), err)) { o.println(err); return true; } }
    else if (a1 == "role") { if (roleArg(a3) < 0) { o.println(tr("Ruoli: admin, oper, guest")); return true; } if (!userSet(userFind(a2), roleArg(a3), -1, err)) { o.println(err); return true; } }
    else if (a1 == "on" || a1 == "off") { if (!userSet(userFind(a2), -1, a1 == "on" ? 1 : 0, err)) { o.println(err); return true; } }
    else { o.println(tr("Uso: user [add <nome> <admin|oper|guest> <password>|del <nome>|role <nome> <ruolo>|on <nome>|off <nome>]")); return true; }
    cfgSave(); o.print(usersText());
  }
  else if (c == "ap") {
    if (a1 == "new") { cfg.apPass = cfgNewApPass(); cfgSave(); netReconfigure(); }
    else if (a1 != "" && a1 != "show") { o.println(tr("Uso: ap [show|new]")); return true; }
    o.println(trf("Hotspot: %s  password: %s", cfg.apSsid.c_str(), cfg.apPass.c_str()));
  }
  else if (c == "locale") {
    String a2 = argAt(line, 2), a3 = argAt(line, 3);
    if (a1 == "country") { a2.toUpperCase(); if (!regionValid(a2)) { o.println(tr("Paese sconosciuto")); return true; } cfg.country = a2; cfgSave(); regionApplyRadio(); }
    else if (a1 == "tx") { int t = a2.toInt(); if (t < 0 || t > 20) { o.println(tr("Potenza: da 2 a 20 dBm (0 = massima consentita)")); return true; } cfg.txDbm = t; cfgSave(); regionApplyRadio(); }
    else if (a1 == "antenna") { if (a2 == "int") { cfg.antExt = 0; } else if (a2 == "ext") { int g = a3.toInt(); if (g < 0 || g > 15) { o.println(tr("Guadagno dell'antenna: da 0 a 15 dBi")); return true; } cfg.antExt = 1; cfg.antGain = g; } else { o.println(tr("Uso: locale antenna int | locale antenna ext <dBi>")); return true; } cfgSave(); regionApplyRadio(); }
    else if (a1 != "") { o.println(tr("Uso: locale [country XX|tx <dBm>|antenna int|ext <dBi>]")); return true; }
    o.print(regionText());
  }
  else if (c == "firewall") {
    String a2 = argAt(line, 2), err;
    if (a1 == "off") { fwOff("seriale"); }
    else if (a1 == "confirm") fwConfirm();
    else if (a1 == "mode") { int m = a2.toInt(); if (a2 == "" || m < 0 || m > 3) { o.println(tr("Modi: 0 spento, 1 solo la mia rete, 2 lista consentita, 3 lista bloccati")); return true; } fwTryStart(0); cfg.fwMode = m; cfgSave(); }
    else if (a1 == "add") { uint32_t a, b; if (cfg.fwN >= VOS_FW_MAX || !cfgParseRange(a2, a, b)) { o.println(tr("Indirizzo non valido o lista piena")); return true; } cfg.fw[cfg.fwN].a = a; cfg.fw[cfg.fwN].b = b; cfg.fw[cfg.fwN].on = true; cfg.fw[cfg.fwN].name = cleanAscii(restFrom(line, 3)); cfg.fwN++; cfgSave(); }
    else if (a1 == "del") { int n = a2.toInt(); if (n < 1 || n > cfg.fwN) { o.println(tr("Voce non trovata")); return true; } for (int i = n - 1; i < cfg.fwN - 1; i++) cfg.fw[i] = cfg.fw[i + 1]; cfg.fwN--; cfgSave(); }
    else if (a1 == "test") { uint32_t ip; if (!ipParse(a2, ip)) { o.println(tr("IP non valido")); return true; } o.println(fwAllow(ip, false) ? tr("ammesso") : tr("rifiutato")); return true; }
    else if (a1 != "" && a1 != "status") { o.println(tr("Uso: firewall [off|confirm|mode <0-3>|add <ip|a-b|rete/nn> [nome]|del <n>|test <ip>]")); return true; }
    o.print(fwText());
  }
  else if (c == "audit") {
    if (a1 == "ack") { auditAck(argAt(line, 2).toInt()); }
    o.print(auditText());
  }
  else if (c == "watchdog") o.print(wdText());
  else if (c == "mesh") {
    String a2 = argAt(line, 2), err;
    if (a1 == "start") { if (!serviceStart("mesh", err)) { o.println(err); return true; } }
    else if (a1 == "stop") serviceStop("mesh", err);
    else if (a1 == "send") { if (!meshSendText(a2, restFrom(line, 3), err)) { o.println(err); return true; } o.println(tr("Inviato")); return true; }
    else if (a1 == "cmd") { if (!meshSendCmd(a2, restFrom(line, 3), err)) { o.println(err); return true; } o.println(tr("Inviato")); return true; }
    else if (a1 == "key") { if (a2 == "new") { cfg.meshKey = meshNewKey(); cfgSave(); } o.println(cfg.meshKey.length() ? cfg.meshKey : String(tr("(nessuna chiave)"))); return true; }
    else if (a1 == "role") { int r = a2.toInt(); if (a2 == "" || r < 0 || r > 2) { o.println(tr("Ruoli: 0 nodo, 1 gateway, 2 sensore")); return true; } cfg.meshRole = r; cfgSave(); }
    else if (a1 != "" && a1 != "status") { o.println(tr("Uso: mesh [start|stop|send <scheda|*> <testo>|cmd <scheda> <azione>|key new|role <0|1|2>]")); return true; }
    o.print(meshText());
  }
  else if (c == "ble") {
    String err;
    if (a1 == "on") { bleSetLimit(argAt(line, 2).toInt()); if (!serviceStart("ble", err)) { o.println(err); return true; } }
    else if (a1 == "off") serviceStop("ble", err);
    o.print(bleText());
  }
  else if (c == "cert") {
    if (a1 == "new") { tlsRegenerate(); o.println(tr("Nuovo certificato pronto: vale dal prossimo riavvio (reboot)")); return true; }
    o.println(trf("HTTPS: %s", cfg.https ? (webHttpsUp() ? tr("attivo") : tr("non partito")) : tr("spento")));
    o.println(trf("Impronta SHA-256: %s", tlsFingerprint().c_str()));
  }
  else if (c == "legal") {
    o.println(String(VOS_NAME) + " " + VOS_VERSION + " - GPL-3.0-or-later / " + tr("licenza commerciale"));
    o.println(tr("Software di terzi e testi delle licenze: comando license, pagina Sistema > Note legali."));
    o.println(tr("Dati salvati sulla scheda: configurazione, utenti (solo impronta della password), registro (150 righe con indirizzi IP), allarmi. Nessun dato va fuori dalla scheda se non attivi tu MQTT o la rete tra schede."));
    o.println(tr("Cancellare tutto: factory-reset oppure tasto BOOT 20 secondi."));
    o.println(tr("Sicurezza: segnala i problemi a paolellad79@gmail.com (risposta entro 7 giorni). Progetto gratuito: gli aggiornamenti non hanno date garantite."));
    o.println(tr("Radio: comando locale. Non e consulenza legale."));
    o.println(tr("Software libero gratuito, fornito fuori da attivita commerciale e senza garanzia (GPL-3.0 sezioni 15-16). Riferimenti normativi: comando license notice."));
  }
  else if (c == "welcome") shellWelcome(o, !cfg.setupDone && serIsOut(o));
  else if (c == "log") {
    if (a1 == "clear") { logClear(); o.println(tr("Registro svuotato")); }
    else if (a1 == "level") {
      String a2 = argAt(line, 2);
      if (a2.length()) { int lv = constrain((int)a2.toInt(), 0, 3); cfg.logLevel = (uint8_t)lv; logSetLevel(lv); cfgSave(); }
      o.println(trf("Livello del registro: %d (0 errori, 1 + attenzioni, 2 + info, 3 + dettagli)", (int)logLevel()));
    }
    else o.print(logGet(a1.length() ? constrain((int)a1.toInt(), 1, 150) : 30));
  }
  else if (c == "reboots") {
    if (a1 == "clear") { diaryClear(); o.println(tr("Diario dei riavvii azzerato")); }
    else { String d = diaryText(20, true); o.print(d.length() ? d : String(tr("Nessun avvio registrato")) + "\n"); }
  }
  else if (c == "factory-reset") {
    o.println(tr("Azzero tutto e riavvio..."));
    vlog("SISTEMA: ripristino di fabbrica dalla shell");
    cfgFactoryReset(); delay(300); ESP.restart();
  }
  else if (c == "airplane" || c == "aereo") {
    if (a1 == "off") { netAirplaneOff("dalla shell"); o.println(tr("Modalita aereo spenta: la rete riparte")); }
    else if (a1 == "on") {
      String m = argAt(line, 2); m.toLowerCase();
      int ex = 0; uint32_t par = 0;
      if (m == "" || m == "boot") ex = 0;
      else if (m == "fisso" || m == "fixed" || m == "manual") ex = 3;
      else if (m.indexOf(':') > 0) { int h = m.toInt(), mi = m.substring(m.indexOf(':') + 1).toInt(); ex = 2; par = (h >= 0 && h < 24 && mi >= 0 && mi < 60) ? h * 60 + mi : 9999; }
      else { long n = m.toInt(); char u = m[m.length() - 1]; ex = 1; par = (u == 'm') ? n * 60 : (u == 'h') ? n * 3600 : n; }
      String err;
      if (netAirplaneOn(ex, par, err)) o.println(netAirplaneText()); else o.println(err);
    }
    else if (a1 == "") o.println(netAirplaneText());
    else o.println(tr("Uso: airplane on [boot|<N>s|<N>m|<N>h|HH:MM|fisso]  /  airplane off"));
  }
  else if (c == "mqtt") {
    if (a1 == "start" || a1 == "restart") { String e; if (!(a1 == "start" ? serviceStart("mqtt", e) : serviceRestart("mqtt", e))) { o.println(e); return true; } o.println(tr("MQTT avviato")); }
    else if (a1 == "stop") { String e; serviceStop("mqtt", e); o.println(tr("MQTT fermato")); }
    else if (a1 == "pub") {
      String tp = argAt(line, 2), msg = restFrom(line, 3);
      if (!tp.length()) { o.println(tr("Uso: mqtt pub <argomento> <testo>")); return true; }
      o.println(mqttPublishRel(tp, msg) ? tr("Inviato") : tr("MQTT non collegato"));
    }
    else if (a1 == "" || a1 == "status") o.print(mqttStatusText());
    else o.println(tr("Uso: mqtt [status|start|stop|restart|pub <argomento> <testo>]"));
  }
  else if (c == "ban") o.print(authBanText());
  else if (c == "unban") {
    if (!a1.length()) { o.println(tr("Uso: unban <indirizzo IP> | unban all")); return true; }
    o.println(authUnban(a1) ? tr("Sbloccato") : tr("Indirizzo non trovato"));
  }
  else if (c == "sleep") {
    int m = a1.toInt();
    if (a1.length() && (m < 1 || m > 10080)) { o.println(tr("Sonno: da 1 minuto a 7 giorni")); return true; }
    if (m) { o.println(trf("Sonno profondo per %d minuti: poi la scheda riparte da sola (o con RESET)", m)); delay(300); powerSleepNow((uint32_t)m * 60UL); }
    else { o.println(tr("Sonno profondo: si riaccende con il tasto RESET")); delay(300); sysSleep(); }
  }
  else if (c == "selftest") {
    String e;
    bool act = line.indexOf("active") >= 0, nm = line.indexOf("names") >= 0;
    if (!selftestStart(act, nm, e)) { o.println(e); return true; }
    int shown = 0;
    while (selftestRunning() || shown < selftestTotal()) {
      String l = selftestLine(shown);
      if (l.length()) { o.println(l); shown++; } else if (!selftestRunning()) break; else delay(100);
    }
    o.print(selftestReport());
  }
  else if (c == "stats") {
    if (a1 == "on") { cfg.statOn = true; statsReset(); cfgSave(); }
    else if (a1 == "off") { cfg.statOn = false; cfgSave(); }
    else if (a1 == "reset") statsReset();
    else if (a1 != "" && a1 != "status") { o.println(tr("Uso: stats [on|off|reset]")); return true; }
    o.print(statsText());
  }
  else if (c == "power") {
    String e;
    if (a1 == "off") powerSet(0, 0, 0, e);
    else if (a1 == "wifi") powerSet(1, 0, 0, e);
    else if (a1 == "cycle") powerSet(2, (uint32_t)argAt(line, 2).toInt(), (uint32_t)argAt(line, 3).toInt(), e);
    else if (a1 != "" && a1 != "status") { o.println(tr("Uso: power [off|wifi|cycle <minuti sveglia> <minuti sonno>]")); return true; }
    if (e.length()) { o.println(e); return true; }
    if (a1 != "" && a1 != "status") { cfgSave(); o.println(tr("Salvato e applicato.")); }
    o.print(powerText());
  }
  else if (c == "reboot") { o.println(tr("Riavvio...")); delay(300); ESP.restart(); }
  else return false;
  return true;
}
