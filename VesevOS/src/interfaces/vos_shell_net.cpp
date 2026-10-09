// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_shell_net.cpp
// Comandi della shell: Wi-Fi, rete, LED, pin, periferiche, lingua, licenze. Aiuti e include: vos_shell_int.h.
#include "vos_shell_int.h"

bool shCmdNet(String c, String a1, const String& line, Print& o, int role) {
  if (c == "wifi" && a1 == "set") {
    String rest = restFrom(line, 2), ss, pw;
    int pp = 0;
    utilTakeArg(rest, pp, ss);                 // la rete puo avere spazi se e tra virgolette
    while (pp < (int)rest.length() && rest[pp] == ' ') pp++;
    if (pp < (int)rest.length() && rest[pp] == '"') utilTakeArg(rest, pp, pw);   // password tra virgolette (anche con spazi)
    else pw = rest.substring(pp);                                                // altrimenti tutto il resto
    if (!ss.length() || ss.length() > 32) { o.println(tr("Uso: wifi set <rete> [password]   (con spazi: wifi set \"mia rete\" password)")); return true; }
    if (pw.length() && (pw.length() < 8 || pw.length() > 63)) { o.println(tr("Password Wi-Fi: da 8 a 63 caratteri")); return true; }
    cfg.staEnabled = true; cfg.staSsid = ss; cfg.staPass = pw; cfg.staDhcp = true;
    bool ntpBack = cfgNtpAfterWifi();
    cfgSave(); netReconfigure(); if (ntpBack) timeApply();
    o.println(trf("Mi collego a %s: se non riesce l'hotspot torna da solo", ss.c_str()));
  }
  else if (c == "wifi" && a1 == "off") { cfg.staEnabled = false; cfgSave(); netReconfigure(); o.println(tr("Wi-Fi di casa scollegata: resta l'hotspot")); }
  else if (c == "setup" && a1 == "done") {            // scheda gia in uso (aggiornata dalla 1.7.1): segna la guida come finita
    cfg.setupDone = true; ledSetSetup(false); cfgSetOrigin("seriale setup done");
    o.println(cfgSave() ? tr("Guida segnata come finita.") : tr("Salvataggio fallito: la guida risultera ancora non finita."));
  }
  else if (c == "setup") wzStart(o);
  else if (c == "diag") {
    o.println(trf("Guida finita:    %s", cfg.setupDone ? tr("si") : tr("NO (hotspot e HTTP forzati accesi a ogni avvio)")));
    o.println(trf("Ultimo salvataggio configurazione: %s", cfgLastSaveOk() ? tr("riuscito") : tr("FALLITO")));
    o.println(trf("Rete: stato %d, modo Wi-Fi %d (1 client, 2 hotspot, 3 entrambi, 0 spento)", (int)netState(), (int)drvWifiMode()));
    o.println(trf("Servizi: hotspot %s, DHCP %s, portale %s, mDNS %s", cfg.apOn ? tr("acceso") : tr("spento"), cfg.dhcpOn ? tr("acceso") : tr("spento"), netCaptive() ? tr("acceso") : tr("spento"), cfg.mdnsOn ? tr("acceso") : tr("spento")));
    o.println(trf("Web: HTTP %s porta %u, HTTPS %s porta %u (in funzione: %s)", cfg.httpOn ? tr("acceso") : tr("spento"), (unsigned)cfg.httpPort, cfg.https ? tr("acceso") : tr("spento"), (unsigned)cfg.httpsPort, webHttpsUp() ? tr("si") : tr("no")));
    o.println(trf("RAM libera %u KB, minima %u KB, blocco piu grande %u KB, PSRAM libera %u KB", (unsigned)(ESP.getFreeHeap() / 1024), (unsigned)(ESP.getMinFreeHeap() / 1024), (unsigned)(ESP.getMaxAllocHeap() / 1024), (unsigned)(ESP.getFreePsram() / 1024)));
    o.println(trf("/flash usati %u KB su %u KB", (unsigned)(drvFsUsed() / 1024), (unsigned)(drvFsTotal() / 1024)));
    o.println(trf("Blocchi grandi (TLS) in PSRAM: %s", g_extmem ? tr("attivo") : tr("non attivo")));
    { String pk; if (VOS_WITH_BLE) pk += "BLE "; if (VOS_WITH_MQTT) pk += "MQTT "; if (VOS_WITH_MESH) pk += "ESP-NOW "; if (VOS_WITH_MFA) pk += "MFA "; if (VOS_WITH_STATS) pk += "Statistiche"; pk.trim(); o.println(trf("Package nel firmware: %s", pk.length() ? pk.c_str() : "-")); }
    o.println(trf("Buffer HTTPS (TLS) in PSRAM: %s", ramTlsState() > 0 ? tr("attivo") : ramTlsState() < 0 ? tr("non disponibile in questo core") : tr("non attivo")));
    o.println(trf("Utente password impostata: %s   Ora valida: %s", authIsSet() ? tr("si") : tr("no"), timeValid() ? tr("si") : tr("no")));
  }
  else if (c == "svc") {
    String a2 = argAt(line, 2);
    if (a1 == "") {
      o.println(trf("Hotspot (AP):  %s", cfg.apOn ? tr("acceso") : tr("spento")));
      o.println(trf("Portale auto:  %s", cfg.apCaptive ? tr("acceso") : tr("spento")));
      o.println(trf("HTTP:          %s, porta %u", cfg.httpOn ? tr("acceso") : tr("spento"), (unsigned)cfg.httpPort));
      o.println(trf("HTTPS:         %s, porta %u", cfg.https ? tr("acceso") : tr("spento"), (unsigned)cfg.httpsPort));
      o.println(trf("DHCP hotspot:  %s, durata %u minuti", cfg.dhcpOn ? tr("acceso") : tr("spento"), (unsigned)cfg.dhcpLease));
      o.println(trf("mDNS:          %s", cfg.mdnsOn ? tr("acceso") : tr("spento")));
      return true;
    }
    bool apOn = cfg.apOn, cap = cfg.apCaptive, hOn = cfg.httpOn, sOn = cfg.https; int hp = cfg.httpPort, sp = cfg.httpsPort;
    bool onoff = (a2 == "on" || a2 == "off"), v = (a2 == "on");
    if (a1 == "ap" && onoff) apOn = v;
    else if (a1 == "captive" && onoff) cap = v;
    else if (a1 == "http" && onoff) hOn = v;
    else if (a1 == "https" && onoff) sOn = v;
    else if (a1 == "http-port" && a2.length()) hp = a2.toInt();
    else if (a1 == "https-port" && a2.length()) sp = a2.toInt();
    else if ((a1 == "dhcp" || a1 == "mdns") && onoff) {            // si applicano subito
      if (a1 == "dhcp") { if (!v && netState() != NET_CLIENT_OK) { o.println(tr("Spegni il DHCP dell'hotspot solo quando la scheda e collegata alla Wi-Fi di casa: senza DHCP il telefono dovrebbe avere un indirizzo fisso")); return true; } cfg.dhcpOn = v; }
      else cfg.mdnsOn = v;
      cfgSave(); netApplyServices(); o.println(tr("Salvato e applicato.")); return true;
    }
    else { o.println(tr("Uso: svc [ap|captive|http|https|dhcp|mdns on|off] [http-port|https-port <numero>]")); return true; }
    String e = cfgSvcCheck(apOn, hOn, hp, sOn, sp);
    if (e.length()) { o.println(e); return true; }
    if (!apOn && cfg.apOn) cfg.dhcpOn = false; else if (apOn && !cfg.apOn) cfg.dhcpOn = true;   // il DHCP segue il punto di accesso
    cfg.apOn = apOn; cfg.apCaptive = cap; cfg.httpOn = hOn; cfg.https = sOn; cfg.httpPort = hp; cfg.httpsPort = sp;
    cfgSave();
    o.println(tr("Salvato. HTTP, HTTPS e porte valgono dal prossimo riavvio (reboot)."));
  }
  else if (c == "ip" || c == "wifi") o.println(netStatusJson());
  else if (c == "net") o.print(netInfoText());
  else if (c == "info") {
    o.println(String(VOS_NAME) + " " + VOS_VERSION);
    o.println(trf("Chip:        %s rev %d, %d core, %u MHz", ESP.getChipModel(), (int)ESP.getChipRevision(), (int)ESP.getChipCores(), (unsigned)getCpuFrequencyMhz()));
    o.println(trf("Acceso da:   %s", uptimeStr(sysUptimeSec()).c_str()));
    o.println(trf("RAM   libera %u KB su %u KB", (unsigned)(ESP.getFreeHeap() / 1024), (unsigned)(ESP.getHeapSize() / 1024)));
    o.println(trf("PSRAM libera %u KB su %u KB", (unsigned)(ESP.getFreePsram() / 1024), (unsigned)(ESP.getPsramSize() / 1024)));
    o.println(trf("/flash  usati %u KB su %u KB", (unsigned)(drvFsUsed() / 1024), (unsigned)(drvFsTotal() / 1024)));
    o.println(trf("Temperatura CPU: %s", fmtTemp(sysCpuTemp()).c_str()));
    o.print(netInfoText());
  }
  else if (c == "wifi-scan") {
    netScanStart(); delay(300);
    for (int i = 0; i < 30 && netScanJson().indexOf("\"running\":true") >= 0; i++) delay(300);
    o.println(netScanJson());
  }
  else if (c == "led") {
    if (a1 == "state") cfg.ledMode = LED_STATE;
    else if (a1 == "heartbeat") cfg.ledMode = LED_HEARTBEAT;
    else if (a1 == "fixed") cfg.ledMode = LED_FIXED;
    else if (a1 == "off") cfg.ledMode = LED_OFF;
    else { o.println(tr("Uso: led state|heartbeat|fixed|off")); return true; }
    ledApplyConfig(); cfgSave(); o.println(trf("LED: %s", a1.c_str()));
  }
  else if (c == "led-color") {
    if (a1.length() != 6) { o.println(tr("Uso: led-color RRGGBB")); return true; }
    cfg.ledColor = (uint32_t)strtoul(a1.c_str(), NULL, 16) & 0xFFFFFF;
    cfgSave(); o.println(trf("Colore: %s", hex6(cfg.ledColor).c_str()));
  }
  else if (c == "led-bright") {
    int v = a1.toInt();
    if (!a1.length() || v < 0 || v > 255) { o.println(tr("Uso: led-bright 0-255")); return true; }
    cfg.ledBrightness = v; cfgSave(); o.println(trf("Luminosita: %d", v));
  }
  else if (c == "hostname") {
    if (!a1.length()) { o.println(cfg.hostname); return true; }
    a1.toLowerCase();
    if (!hostnameValid(a1)) { o.println(tr("Nome non valido (1-32: lettere, numeri, trattino)")); return true; }
    cfg.hostname = a1; cfgSave(); netReconfigure(); o.println(trf("Nome host: %s", a1.c_str()));
  }
  else if (c == "domain") {
    if (!a1.length()) { o.println(cfg.domain.length() ? cfg.domain : String(tr("(nessun dominio)"))); return true; }
    if (a1 == "-") a1 = "";
    a1.toLowerCase();
    if (!domainValid(a1)) { o.println(tr("Dominio non valido")); return true; }
    cfg.domain = a1; cfgSave(); o.println(a1.length() ? trf("Dominio: %s", a1.c_str()) : String(tr("Dominio tolto")));
  }
  else if (c == "lang") {
    if (!a1.length()) {
      o.println(trf("Lingua: %s", cfg.lang.c_str()));
      o.println(trf("Lingue installate: %s", langListJson().c_str()));
      return true;
    }
    a1.toLowerCase();
    String err;
    if (!langSet(a1, err)) { o.println(err); return true; }
    o.println(trf("Lingua: %s", cfg.lang.c_str()));
  }
  else if (c == "license" || c == "licenza") {
    String id = a1.length() ? a1 : String("notice");
    int li = licFind(id);
    if (li < 0) { o.println(trf("Licenza sconosciuta. Disponibili: %s", licIds().c_str())); return true; }
    if (licZ(li)) { o.println(tr("Il testo completo delle licenze si legge dalla pagina (Sistema > Note legali) e nel file LICENSE del progetto.")); return true; }
    const char* tx = licText(li); size_t n = licSize(li);
    for (size_t off = 0; off < n; off += 256) {            // a blocchi, senza copiare tutto in RAM
      size_t k = (n - off < 256) ? (n - off) : 256;
      o.write((const uint8_t*)(tx + off), k);
    }
    o.println();
  }
  else if (c == "pin") {
    String a1 = argAt(line, 1), a2 = argAt(line, 2), a3 = argAt(line, 3);
    if (a1 == "") { o.println(tr("Uso: pin <numero> [high|low|blink|read [up|down]|off]")); return true; }
    int g = a1.toInt();
    if (a2 == "") {
      const char* why = pinTestBlock(g);
      o.println(why ? trf("GPIO%d: non provabile (%s)", g, why) : trf("GPIO%d: libero, si puo provare", g));
      return true;
    }
    String err;
    if (!pinTestRequest(g, a2, a3, err)) { o.println(trf("Errore: %s", err.c_str())); return true; }
    o.println(trf("GPIO%d: %s - si spegne da solo dopo qualche secondo", g, a2.c_str()));
  }
  else if (c == "dev") {
    String a2 = argAt(line, 2), err, out;
    if (a1 == "" || a1 == "list") { o.print(devListText()); return true; }
    if (role < ROLE_ADMIN && a1 != "status" && a1 != "act") { o.println(tr("Il tuo ruolo non permette questo comando")); return true; }
    if (a1 == "status") {
      String j = devStatusJson(a2, role);
      if (!j.length()) { o.println(tr("Periferica sconosciuta o ruolo insufficiente")); return true; }
      o.println(j); return true;
    }
    if (a1 == "on" || a1 == "off") {
      if (!devSet(a2, a1 == "on", err)) { o.println(err); return true; }
      o.print(devListText()); return true;
    }
    if (a1 == "act") {
      if (!devAct(a2, argAt(line, 3), restFrom(line, 4), role, out, err)) { o.println(err); return true; }
      o.println(out.length() ? out : String("ok")); return true;
    }
    o.println(tr("Uso: dev [status <nome>|on <nome>|off <nome>|act <nome> <azione> [arg]]"));
  }
  else if (c == "pins") {
    for (int i = 0; i < pinCount(); i++) {
      const PinInfo* p = pinAt(i);
      o.println("GPIO" + String(p->gpio) + "  " + tr(p->owner) + "  " + tr(p->note));
    }
  }
  else return false;
  return true;
}
