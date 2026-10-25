// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_shell_net.cpp
// Comandi della shell: Wi-Fi, rete, LED, pin, periferiche, lingua, licenze. Aiuti e include: vos_shell_int.h.
#include "../devices/vos_io.h"
#include "../devices/vos_mod.h"
#include "../net/vos_wps.h"
#include "../packages/vos_btn.h"
#include "vos_shell_int.h"
#include "vos_shell_ask.h"
#include "../drivers/vos_drv_time.h"
#include "../core/vos_strbuf.h"
#include "../drivers/vos_drv_cpu.h"
#include "../drivers/vos_drv_heap.h"
#include "../drivers/vos_drv_sys.h"

// Legge una parola di 'rest' da pos. Tra virgolette puo avere spazi (\" per una virgoletta). Se non ci sta in cap, big = true.
static void takeArg(const char* rest, size_t& pos, char* out, size_t cap, bool& big) {
  size_t n = 0;
  big = false;
  while (rest[pos] == ' ') pos++;
  bool quoted = (rest[pos] == '"');
  if (quoted) pos++;
  while (rest[pos] && (quoted ? rest[pos] != '"' : rest[pos] != ' ')) {
    char ch = rest[pos];
    if (quoted && ch == '\\' && rest[pos + 1] == '"') { ch = '"'; pos++; }
    if (n + 1 < cap) out[n++] = ch; else big = true;
    pos++;
  }
  if (quoted && rest[pos]) pos++;                 // salta la virgoletta finale
  out[n] = 0;
}

// Elenco delle periferiche per la shell (se la memoria finisce si stampa quel che c'e)
static void devListPrint(OutSink& o) {
  HeapBuf t(DEV_JSON_MAX);
  devListText(t);
  o.print(t.c_str());
}

// wifi set: salva la rete di casa e si collega. Password vuota = rete aperta.
void shx::shWifiSetApply(const char* ssid, const char* pass, OutSink& o) {
  size_t pwLen = strlen(pass);
  if (pwLen && (pwLen < 8 || pwLen > 63)) { o.println(tr("Password Wi-Fi: da 8 a 63 caratteri")); return; }
  cfg.staEnabled = true; cfg.staSsid = ssid; cfg.staPass = pass; cfg.staDhcp = true;
  bool ntpBack = cfgNtpAfterWifi();
  cfgSave(); netReconfigure(); if (ntpBack) timeApply();
  o.println(trf("Mi collego a %s: se non riesce l'hotspot torna da solo", ssid));
}

// Le classi di io hanno nomi inglesi (relay, input, analog); i nomi italiani (rele, ingresso, analogico) restano validi.
// ioAdd conosce solo i nomi interni, quindi qui si traduce.
static const char* ioClassName(const Word& w) {
  if (w == "relay") return "rele";
  if (w == "input") return "ingresso";
  if (w == "analog") return "analogico";
  return w.c_str();
}

bool shCmdNet(const Word& c, const Word& a1, const char* line, OutSink& o, int role) {
  if (c == "wifi" && a1 == "set") {
    const char* rest = restFrom(line, 2);
    char ss[Word::MAX + 1], pw[Word::MAX + 1];
    bool ssBig = false, pwBig = false;
    size_t pp = 0;
    takeArg(rest, pp, ss, sizeof ss, ssBig);                 // la rete puo avere spazi se e tra virgolette
    while (rest[pp] == ' ') pp++;
    bool pwInLine = (rest[pp] != 0);                         // c'e qualcosa dopo il nome: la password scritta nella riga (anche "" = rete aperta)
    size_t ssLen = strlen(ss);
    if (!ssLen || ssLen > 32 || ssBig) { o.println(tr("Uso: wifi set <rete>   (con spazi: wifi set \"mia rete\"; la password te la chiedo io. Rete aperta: wifi set <rete> \"\")")); return true; }
    if (!pwInLine) {                                         // senza password nella riga: arriva dal campo a parte della pagina, oppure si chiede
      const char* given = askSecretGiven(o);
      if (given) shWifiSetApply(ss, given, o);
      else askSecret(o, SECRET_WIFI_SET, ss, 0, false);
      return true;
    }
    if (rest[pp] == '"') takeArg(rest, pp, pw, sizeof pw, pwBig);   // password tra virgolette (anche con spazi)
    else { pwBig = strlen(rest + pp) >= sizeof pw; snprintf(pw, sizeof pw, "%s", rest + pp); }   // altrimenti tutto il resto
    if (pwBig) { o.println(tr("Password Wi-Fi: da 8 a 63 caratteri")); return true; }
    if (pw[0]) shWarnPassInLine(o);                          // vecchia sintassi, per gli script
    shWifiSetApply(ss, pw, o);
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
    o.println(trf("RAM libera %u KB, minima %u KB, blocco piu grande %u KB, PSRAM libera %u KB", (unsigned)(drvHeapFree() / 1024), (unsigned)(drvHeapMinFree() / 1024), (unsigned)(drvHeapMaxBlock() / 1024), (unsigned)(drvPsramFree() / 1024)));
    o.println(trf("/flash usati %u KB su %u KB", (unsigned)(drvFsUsed() / 1024), (unsigned)(drvFsTotal() / 1024)));
    o.println(trf("Blocchi grandi (TLS) in PSRAM: %s", g_extmem ? tr("attivo") : tr("non attivo")));
    {
      char pk[64]; StrBuf p(pk, sizeof pk);
      if (VOS_WITH_BLE) p.add("BLE ");
      if (VOS_WITH_MQTT) p.add("MQTT ");
      if (VOS_WITH_MESH) p.add("ESP-NOW ");
      if (VOS_WITH_MFA) p.add("MFA ");
      if (VOS_WITH_STATS) p.add("Statistiche ");
      if (VOS_WITH_WPS) p.add("WPS");
      size_t n = p.length(); while (n && pk[n - 1] == ' ') pk[--n] = 0;      // senza lo spazio in coda
      o.println(trf("Package nel firmware: %s", n ? pk : "-"));
    }
    o.println(trf("Buffer HTTPS (TLS) in PSRAM: %s", ramTlsState() > 0 ? tr("attivo") : ramTlsState() < 0 ? tr("non disponibile in questo core") : tr("non attivo")));
    o.println(trf("Utente password impostata: %s   Ora valida: %s", authIsSet() ? tr("si") : tr("no"), timeValid() ? tr("si") : tr("no")));
  }
  else if (c == "svc") {
    Word a2 = argAt(line, 2);
    if (a1 == "") {
      o.println(trf("Hotspot (AP):  %s", cfg.apOn ? tr("acceso") : tr("spento")));
      o.println(trf("Portale auto:  %s", cfg.apCaptive ? tr("acceso") : tr("spento")));
      o.println(trf("HTTP:          %s, porta %u", cfg.httpOn ? tr("acceso") : tr("spento"), (unsigned)cfg.httpPort));
      o.println(trf("HTTPS:         %s, porta %u", cfg.https ? tr("acceso") : tr("spento"), (unsigned)cfg.httpsPort));
      o.println(trf("Redirect:       %s (HTTP verso HTTPS, serve HTTP e HTTPS accesi)", cfg.httpRedirect ? tr("acceso") : tr("spento")));
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
    else if (a1 == "http-port" && !a2.empty()) hp = a2.toInt();
    else if (a1 == "https-port" && !a2.empty()) sp = a2.toInt();
    else if (a1 == "redirect" && onoff) {                           // vale subito; nel registro modifiche come ogni salvataggio
      cfg.httpRedirect = v; cfgSave();
      o.println(tr("Salvato e applicato."));
      if (v && !(cfg.httpOn && cfg.https)) o.println(tr("Il redirect agisce solo con HTTP e HTTPS tutti e due accesi."));
      return true;
    }
    else if ((a1 == "dhcp" || a1 == "mdns") && onoff) {            // si applicano subito
      if (a1 == "dhcp") { if (!v && netState() != NET_CLIENT_OK) { o.println(tr("Spegni il DHCP dell'hotspot solo quando la scheda e collegata alla Wi-Fi di casa: senza DHCP il telefono dovrebbe avere un indirizzo fisso")); return true; } cfg.dhcpOn = v; }
      else cfg.mdnsOn = v;
      cfgSave(); netApplyServices(); o.println(tr("Salvato e applicato.")); return true;
    }
    else { o.println(tr("Uso: svc [ap|captive|http|https|redirect|dhcp|mdns on|off] [http-port|https-port <numero>]")); return true; }
    const char* why = cfgSvcCheck(apOn, hOn, hp, sOn, sp);
    if (why) { o.println(why); return true; }
    if (!apOn && cfg.apOn) cfg.dhcpOn = false; else if (apOn && !cfg.apOn) cfg.dhcpOn = true;   // il DHCP segue il punto di accesso
    cfg.apOn = apOn; cfg.apCaptive = cap; cfg.httpOn = hOn; cfg.https = sOn; cfg.httpPort = hp; cfg.httpsPort = sp;
    cfgSave();
    o.println(tr("Salvato. HTTP, HTTPS e porte valgono dal prossimo riavvio (reboot)."));
  }
  else if ((c == "ip" || c == "wifi") && a1 != "scan") { HeapBuf j(NET_STATUS_JSON_MAX); netStatusJson(j); o.println(j.c_str()); }
  else if (c == "net") o.print(netInfoText().c_str());
  else if (c == "info") {
    o.printf("%s %s\r\n", VOS_NAME, VOS_VERSION);
    o.println(trf("Chip:        %s rev %d, %d core, %u MHz", drvChipModel(), (int)drvChipRevision(), (int)drvChipCores(), (unsigned)drvCpuMhz()));
    { char up[64]; uptimeTo(sysUptimeSec(), up, sizeof up); o.println(trf("Acceso da:   %s", up)); }
    o.println(trf("RAM   libera %u KB su %u KB", (unsigned)(drvHeapFree() / 1024), (unsigned)(drvHeapTotal() / 1024)));
    o.println(trf("PSRAM libera %u KB su %u KB", (unsigned)(drvPsramFree() / 1024), (unsigned)(drvPsramTotal() / 1024)));
    o.println(trf("/flash  usati %u KB su %u KB", (unsigned)(drvFsUsed() / 1024), (unsigned)(drvFsTotal() / 1024)));
    char tt[16]; fmtTemp(sysCpuTemp(), tt, sizeof tt);
    o.println(trf("Temperatura CPU: %s", tt));
    o.print(netInfoText().c_str());
  }
  else if (c == "wifi-scan" || (c == "wifi" && a1 == "scan")) {      // wifi-scan e il vecchio nome
    netScanStart(); drvDelayMs(300);
    for (int i = 0; i < 30 && scanRunning(); i++) drvDelayMs(300);
    HeapBuf j(NET_SCAN_JSON_MAX);
    if (netScanJson(j)) o.println(j.c_str()); else o.println(tr("Memoria insufficiente"));
  }
  else if (c == "hostname") {
    if (a1.empty()) { o.println(cfg.hostname.c_str()); return true; }
    Word h = a1; h.lower();
    if (!hostnameValid(h.c_str())) { o.println(tr("Nome non valido (1-32: lettere, numeri, trattino)")); return true; }
    cfg.hostname = h.c_str(); cfgSave(); netReconfigure(); o.println(trf("Nome host: %s", h.c_str()));
  }
  else if (c == "domain") {
    if (a1.empty()) { if (!cfg.domain.empty()) o.println(cfg.domain.c_str()); else o.println(tr("(nessun dominio)")); return true; }
    Word d = (a1 == "-") ? Word() : a1;
    d.lower();
    if (!domainValid(d.c_str())) { o.println(tr("Dominio non valido")); return true; }
    cfg.domain = d.c_str(); cfgSave();
    if (d.empty()) o.println(tr("Dominio tolto")); else o.println(trf("Dominio: %s", d.c_str()));
  }
  else if (c == "lang") {
    if (a1.empty()) {
      o.println(trf("Lingua: %s", cfg.lang.c_str()));
      HeapBuf j(LANG_JSON_MAX);
      if (langListJson(j)) o.println(trf("Lingue installate: %s", j.c_str()));
      return true;
    }
    Word l = a1; l.lower();
    ErrBuf err;
    if (!langSet(l.c_str(), err)) { o.println(err.c_str()); return true; }
    o.println(trf("Lingua: %s", cfg.lang.c_str()));
  }
  else if (c == "license") {
    int li = licFind(a1.empty() ? "notice" : a1.c_str());
    if (li < 0) { char ids[96]; licIds(ids, sizeof(ids)); o.println(trf("Licenza sconosciuta. Disponibili: %s", ids)); return true; }
    if (licZ(li)) { o.println(tr("Il testo completo delle licenze si legge dalla pagina (Informazioni > Note legali) e nel file LICENSE del progetto.")); return true; }
    const char* tx = licText(li); size_t n = licSize(li);
    for (size_t off = 0; off < n; off += 256) {            // a blocchi, senza copiare tutto in RAM
      size_t k = (n - off < 256) ? (n - off) : 256;
      o.write((const uint8_t*)(tx + off), k);
    }
    o.println();
  }
  else if (c == "pin") {
    Word a2 = argAt(line, 2), a3 = argAt(line, 3);
    if (a1 == "") { o.println(tr("Uso: pin <numero> [high|low|blink|read [up|down]|off]")); return true; }
    int g = a1.toInt();
    if (a2 == "") {
      const char* why = pinTestBlock(g);
      o.println(why ? trf("GPIO%d: non provabile (%s)", g, why) : trf("GPIO%d: libero, si puo provare", g));
      return true;
    }
    ErrBuf err;
    if (!pinTestRequest(g, a2.c_str(), a3.c_str(), err)) { o.println(trf("Errore: %s", err.c_str())); return true; }
    o.println(trf("GPIO%d: %s - si spegne da solo dopo qualche secondo", g, a2.c_str()));
  }
  else if (c == "dev") {
    Word a2 = argAt(line, 2);
    HeapBuf out(512); ErrBuf err;
    if (a1 == "" || a1 == "list") { devListPrint(o); return true; }
    if (role < ROLE_ADMIN && a1 != "status" && a1 != "act") { o.println(tr("Il tuo ruolo non permette questo comando")); return true; }
    if (a1 == "status") {
      HeapBuf j(DEV_JSON_MAX);
      if (!devStatusJson(a2.c_str(), role, j)) { o.println(j.oom() || j.over() ? tr("Memoria insufficiente") : tr("Periferica sconosciuta o ruolo insufficiente")); return true; }
      o.println(j.c_str()); return true;
    }
    if (a1 == "on" || a1 == "off") {
      if (!devSet(a2.c_str(), a1 == "on", err)) { o.println(err.c_str()); return true; }
      devListPrint(o); return true;
    }
    if (a1 == "act") {
      if (!devAct(a2.c_str(), argAt(line, 3).c_str(), restFrom(line, 4), role, out, err)) { o.println(err.c_str()); return true; }
      if (out.length()) o.println(out.c_str()); else o.println("ok");
      return true;
    }
    o.println(tr("Uso: dev [status <nome>|on <nome>|off <nome>|act <nome> <azione> [arg]]"));
  }
#if VOS_WITH_SD
  else if (c == "sd") shSd(line, o, role);
#endif
  else if (c == "io") {
    if (role < ROLE_ADMIN) { o.println(tr("Il tuo ruolo non permette questo comando")); return true; }
    Word a2 = argAt(line, 2), a3 = argAt(line, 3);
    ErrBuf err;
    if (a1 == "" || a1 == "list") {
      HeapBuf txt(IO_JSON_MAX);                                       // il testo si scrive qui (heap), non sullo stack
      if (!ioText(txt)) o.println(tr("Memoria insufficiente")); else o.print(txt.c_str());
      return true;
    }
    if (a1 == "add") {
      Word nm;
      char outN[16];
      char optBuf[160]; StrBuf opt(optBuf, sizeof optBuf);
      for (int k = 4; k <= 8; k++) {
        Word t = argAt(line, k); t.lower();
        if (t == "inv" || t == "up" || t == "down" || t == "fast" || t.startsWith("scl=") || t.startsWith("addr=") || t.startsWith("drv=")) opt.add(t.c_str()).add(',');
        else if (!t.empty() && nm.empty()) nm = t;
      }
      if (opt.truncated()) { o.println(tr("Comando troppo lungo")); return true; }
      if (!ioAdd(ioClassName(a2), a3.toInt(), nm.c_str(), opt.c_str(), outN, err)) { o.println(err.c_str()); return true; }
      o.println(trf("Creata: %s", outN)); return true;
    }
    if (a1 == "del") {
      if (!ioRemove(a2.c_str(), err)) { o.println(err.c_str()); return true; }
      auditEvent(AUD_INFO, "", trf("periferica %s tolta (%s)", a2.c_str(), "terminale").c_str()); o.println("ok"); return true;
    }
    if (a1 == "rename") { if (!ioRename(a2.c_str(), a3.c_str(), err)) { o.println(err.c_str()); return true; } o.println("ok"); return true; }
    if (a1 == "set") { if (!ioSet(a2.c_str(), a3.toInt() || a3 == "on", role, err)) { o.println(err.c_str()); return true; } o.println("ok"); return true; }
    if (a1 == "get") { int v = 0; if (!ioGet(a2.c_str(), v, err)) { o.println(err.c_str()); return true; } o.println(v); return true; }
    o.println(tr("Uso: io [list|add <classe> <pin> [nome] [inv|up|down|scl=N addr=0x76]|del <nome>|rename <nome> <nuovo>|set <nome> <0|1>|get <nome>]"));
  }
  else if (c == "btn") {
    if (a1 == "" || a1 == "status") { o.print(btnText().c_str()); return true; }
    if (role < ROLE_ADMIN) { o.println(tr("Il tuo ruolo non permette questo comando")); return true; }
    Word a2 = argAt(line, 2);
    ErrBuf err;
    int act = (a2 == "none" || a2 == "niente") ? BA_NONE : (a2 == "airplane" || a2 == "aereo") ? BA_AIR_OFF
              : a2 == "wps" ? BA_WPS : (a2 == "find" || a2 == "trova") ? BA_FIND : -1;      // i nomi italiani restano validi
    if ((a1 == "1" || a1 == "2" || a1 == "3") && act >= 0) {
      if (!btnSetAction(a1.toInt(), act, err)) { o.println(err.c_str()); return true; }
      cfgSetOrigin("shell"); cfgSave(); o.print(btnText().c_str()); return true;
    }
    o.println(tr("Uso: btn [1|2|3 none|airplane|wps|find]"));
  }
  else if (c == "wps") {
    if (role < ROLE_ADMIN) { o.println(tr("Il tuo ruolo non permette questo comando")); return true; }
    Word a2 = argAt(line, 2);
    ErrBuf err;
    bool on2 = (a2 == "on"), off2 = (a2 == "off");
    if (a1 == "" || a1 == "status") { o.print(wpsText().c_str()); return true; }
    if (a1 == "on" || a1 == "off") { if (!wpsSetOn(a1 == "on", err)) { o.println(err.c_str()); return true; } o.print(wpsText().c_str()); return true; }
    if (a1 == "pin" && (on2 || off2)) {
      cfg.wpsPin = on2;
      cfgSetOrigin("shell"); cfgSave(); o.print(wpsText().c_str()); return true;
    }
    if (a1 == "start") {
      if (!wpsStart(a2 == "pin", "shell", err)) { o.println(err.c_str()); return true; }
      o.println(a2 == "pin" ? tr("WPS con PIN avviato: guarda il PIN con 'wps' e scrivilo nel router entro 2 minuti.") : tr("WPS avviato: premi il tasto WPS del router entro 2 minuti."));
      return true;
    }
    if (a1 == "stop") { wpsStop(); o.println(tr("WPS annullato")); return true; }
    o.println(tr("Uso: wps [status|on|off|pin on|off|start [pin]|stop]"));
  }
  else if (c == "mod") {
    if (role < ROLE_ADMIN) { o.println(tr("Il tuo ruolo non permette questo comando")); return true; }
    Word a2 = argAt(line, 2), a3 = argAt(line, 3);
    HeapBuf out(2048); ErrBuf err;
    if (a1 == "" || a1 == "list") { HeapBuf t(MOD_TEXT_MAX); modListText(t); o.print(t.c_str()); return true; }
    if (a1 == "info") { HeapBuf t(MOD_TEXT_MAX); modInfoText(a2.c_str(), t); o.print(t.c_str()); return true; }
    if (a1 == "install") { if (!modInstall(a2.c_str(), a3.c_str(), out, err)) { o.println(err.c_str()); return true; } o.println(out.c_str()); return true; }
    if (a1 == "del") { if (!modDelete(a2.c_str(), err)) { o.println(err.c_str()); return true; } o.println("ok"); return true; }
    o.println(tr("Uso: mod [list|info <nome>|install <nome> [prefisso]|del <nome>]"));
  }
  else if (c == "pins") {
    for (int i = 0; i < pinCount(); i++) {
      const PinInfo* p = pinAt(i);
      o.printf("GPIO%d  %s  %s\r\n", (int)p->gpio, tr(p->owner), tr(p->note));
    }
  }
  else return false;
  return true;
}
