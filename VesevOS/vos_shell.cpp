// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_shell.cpp
#include "vos_shell.h"
#include "vos_rules.h"
#include "vos_boot.h"
#include "vos_common.h"
#include "vos_config.h"
#include "vos_auth.h"
#include "vos_sys.h"
#include "vos_net.h"
#include "vos_mqtt.h"
#include "vos_led.h"
#include "vos_pins.h"
#include "vos_util.h"
#include "vos_files.h"
#include "vos_time.h"
#include "vos_log.h"
#include "vos_i18n.h"
#include "vos_license.h"
#include "vos_region.h"
#include "vos_audit.h"
#include "vos_fw.h"
#include "vos_wd.h"
#include "vos_mesh.h"
#include "vos_ble.h"
#include "vos_tls.h"
#include "vos_web.h"
#include "vos_power.h"
#include "vos_stats.h"
#include "vos_selftest.h"
extern bool g_extmem;                 // definita in VesevOS.ino: i blocchi grandi vanno in PSRAM?
#include <LittleFS.h>
#include <WiFi.h>

static String argAt(const String& s, int idx) {
  int n = 0, i = 0;
  while (i < (int)s.length()) {
    while (i < (int)s.length() && s[i] == ' ') i++;
    int st = i;
    while (i < (int)s.length() && s[i] != ' ') i++;
    if (i > st) { if (n == idx) return s.substring(st, i); n++; }
  }
  return "";
}

static String restFrom(const String& s, int idx) {
  int n = 0, i = 0;
  while (i < (int)s.length()) {
    while (i < (int)s.length() && s[i] == ' ') i++;
    if (i >= (int)s.length()) break;
    if (n == idx) return s.substring(i);
    while (i < (int)s.length() && s[i] != ' ') i++;
    n++;
  }
  return "";
}

static void cmdHelp(Print& o) {
  o.println(tr("Comandi:"));
  o.println(tr("  help            questo elenco"));
  o.println(tr("  uname           versione sistema"));
  o.println(tr("  about           chi siamo e indirizzo GitHub"));
  o.println(tr("  serial-auth [on|off]  password sulla seriale: mostra o cambia"));
  o.println(tr("  uptime          da quanto e acceso (+ motivo reset, avvii)"));
  o.println(tr("  free            memoria RAM/PSRAM"));
  o.println(tr("  df              spazio su flash"));
  o.println(tr("  temp            temperatura CPU"));
  o.println(tr("  top             CPU, RAM, temperatura"));
  o.println(tr("  ps              elenco task"));
  o.println(tr("  kill <nome>     ferma un task (solo quelli consentiti, vedi ps)"));
  o.println(tr("  start <nome>    avvia o riavvia un task fermato (restart = uguale)"));
  o.println(tr("  rules [run <n>] automazioni: elenco, oppure esegui la regola n"));
  o.println(tr("  boot-order [lista|reset]  ordine di avvio dei servizi"));
  o.println(tr("  ls [cartella]   elenco file (flash)"));
  o.println(tr("  cat <file>      mostra un file"));
  o.println(tr("  mkdir <nome>    crea cartella"));
  o.println(tr("  rm <nome>       elimina file o cartella"));
  o.println(tr("  mv <da> <a>     sposta o rinomina"));
  o.println(tr("  cp <da> <a>     copia un file"));
  o.println(tr("  write <file> <testo>   scrive un file di testo"));
  o.println(tr("  date [set AAAA-MM-GG HH:MM]  data e ora (mostra o imposta)"));
  o.println(tr("  ntp [on|off|sync|every <min>]  stato NTP / accendi, spegni / risincronizza / frequenza"));
  o.println(tr("  cpu [auto|80|160|240]  velocita CPU"));
  o.println(tr("  hostname [nome] mostra o cambia il nome host"));
  o.println(tr("  domain [nome|-] mostra, cambia o toglie il dominio"));
  o.println(tr("  info            riassunto: versione, chip, memoria, rete, MAC"));
  o.println(tr("  net             rete: IP, MAC client e AP, segnale"));
  o.println(tr("  ip              rete e indirizzo"));
  o.println(tr("  wifi            stato wi-fi"));
  o.println(tr("  wifi-scan       cerca reti"));
  o.println(tr("  wifi set <rete> [password] | wifi off   collega la Wi-Fi di casa (nome senza spazi) o la scollega"));
  o.println(tr("  setup           configurazione guidata da seriale (lingua, paese, nome, antenna, hotspot, utente, Wi-Fi, ora)"));
  o.println(tr("  setup done      segna la guida come finita (scheda gia in uso)"));
  o.println(tr("  diag            diagnosi: guida, salvataggio, rete, servizi, RAM, flash"));
  o.println(tr("  svc [ap|captive|http|https|dhcp|mdns on|off] [http-port|https-port <n>]  servizi di rete e porte (web e porte dal riavvio, DHCP e mDNS subito)"));
  o.println(tr("  mqtt [start|stop|restart|pub]  servizio MQTT"));
  o.println(tr("  airplane [on <come>|off]  modalita aereo (come: boot, 30s, 10m, 2h, 07:30, fisso)"));
  o.println(tr("  led <modo>      state|heartbeat|fixed|off"));
  o.println(tr("  led-color <RRGGBB>   colore (esadecimale)"));
  o.println(tr("  led-bright <0-255>   luminosita"));
  o.println(tr("  lang [codice]   mostra o cambia la lingua (it, en, ...)"));
  o.println(tr("  license [id]    note legali e licenze (notice, gpl3, lgpl21, apache2, mit, bsd3)"));
  o.println(tr("  pins            pin usati"));
  o.println(tr("  pin <n> [high|low|blink|read [up|down]|off]  prova un pin (si spegne da solo)"));
  o.println(tr("  config          mostra configurazione (senza password)"));
  o.println(tr("  passwd <utente> <nuova>  cambia la password di un utente (min 6 caratteri)"));
  o.println(tr("  user [add <nome> <admin|oper|guest> <password>|del <nome>|role <nome> <ruolo>|on <nome>|off <nome>]  utenti"));
  o.println(tr("  ap [show|new]   hotspot: mostra la password o creane una nuova"));
  o.println(tr("  locale [country XX|tx <dBm>|antenna int|ext <dBi>]  paese, potenza, antenna"));
  o.println(tr("  firewall [off|confirm|mode <0-3>|add <ip|a-b|rete/nn> [nome]|del <n>|test <ip>]  filtro IP"));
  o.println(tr("  audit [ack [n]] allarmi della configurazione e registro modifiche"));
  o.println(tr("  watchdog        stato del watchdog"));
  o.println(tr("  mesh [start|stop|send <scheda|*> <testo>|cmd <scheda> <azione>|key new|role <0|1|2>]  rete tra schede"));
  o.println(tr("  ble [on|off]    Bluetooth per configurare dal telefono (10 minuti)"));
  o.println(tr("  cert [new]      certificato HTTPS: impronta o nuovo certificato"));
  o.println(tr("  legal           note legali: licenze, dati salvati, sicurezza"));
  o.println(tr("  welcome         schermata di benvenuto"));
  o.println(tr("  ban             indirizzi bloccati o sospetti"));
  o.println(tr("  unban <ip|all>  sblocca un indirizzo (o tutti)"));
  o.println(tr("  logout          esce (solo seriale)"));
  o.println(tr("  log [n|clear]   ultime righe del registro (n righe) o svuota"));
  o.println(tr("  factory-reset   azzera tutto (poi riavvia)"));
  o.println(tr("  reboot          riavvia"));
  o.println(tr("  sleep [minuti]  sonno profondo (con i minuti la scheda riparte da sola; senza, solo con RESET)"));
  o.println(tr("  stats [on|off|reset]  statistiche d'uso anonime (spente di fabbrica)"));
  o.println(tr("  selftest [active] [names]  autodiagnosi con report (prove attive: LED e MQTT; names: nomi reali)"));
  o.println(tr("  power [off|wifi|cycle <sveglia> <sonno>]  risparmio energia (minuti)"));
}

static String hex6(uint32_t c) {
  char b[8]; snprintf(b, sizeof(b), "%06lX", (unsigned long)(c & 0xFFFFFF)); return String(b);
}

// comandi permessi all'Operatore (uso della scheda, niente rete, sicurezza, utenti, file di sistema)
static bool operOk(const String& c, const String& a1) {
  static const char* const OK[] = {"help", "?", "uname", "about", "uptime", "free", "df", "temp", "top", "ps", "info", "net", "ip", "wifi",
                                   "date", "led", "led-color", "led-bright", "pin", "pins", "rules", "audit", "watchdog", "legal", "license", "licenza", "locale"};
  for (size_t i = 0; i < sizeof(OK) / sizeof(OK[0]); i++) if (c == OK[i]) return true;
  if (c == "ntp") return a1 == "" || a1 == "sync";
  if (c == "log") return a1 != "clear";
  if (c == "mqtt") return a1 == "" || a1 == "status" || a1 == "pub";
  if (c == "mesh") return a1 == "" || a1 == "status" || a1 == "send" || a1 == "cmd";
  if (c == "airplane" || c == "aereo" || c == "reboot" || c == "sleep") return true;
  return false;
}

static void wzStart(Print& o);
static int roleArg(const String& r) { return r == "admin" ? ROLE_ADMIN : r == "oper" ? ROLE_OPER : r == "guest" ? ROLE_GUEST : -1; }

void shellExec(const String& lineIn, Print& o, int role) {
  String line = lineIn; line.trim();
  if (line.length() == 0) return;
  String c = argAt(line, 0);
  c.toLowerCase();
  String a1 = argAt(line, 1);

  if (role < 0 && authIsSet()) { o.println(tr("Non autorizzato. Fai login.")); return; }
  if (role < 0) role = ROLE_ADMIN;                       // nessuna password ancora: primo avvio
  if (role < ROLE_ADMIN && (role < ROLE_OPER || !operOk(c, a1))) { o.println(tr("Il tuo ruolo non permette questo comando")); return; }

  if (c == "help" || c == "?") cmdHelp(o);
  else if (c == "uname") o.println(String(VOS_NAME) + " " + VOS_VERSION + " (ESP32-S3, core " + ESP.getSdkVersion() + ")");
  else if (c == "about") {
    o.println(String(VOS_NAME) + " " + VOS_VERSION + " - " + tr("Una piattaforma, mille schede."));
    o.println(String("GitHub: ") + VOS_GITHUB);
    o.println(String("(C) 2026 Domenico Paolella - GPL-3.0-or-later / ") + tr("licenza commerciale"));
  }
  else if (c == "serial-auth") {
    String a = a1; a.toLowerCase();
    if (a == "on" || a == "off") {
      cfg.serialAuth = (a == "on"); cfgSave();
      vlog("SICUREZZA: password sulla seriale %s", cfg.serialAuth ? "attivata" : "disattivata");
    } else if (a.length()) { o.println(tr("Uso: serial-auth on|off")); return; }
    o.println(cfg.serialAuth ? tr("Password sulla seriale: ACCESA") : tr("Password sulla seriale: SPENTA"));
  }
  else if (c == "uptime") {
    o.println(trf("Acceso da: %s", uptimeStr(sysUptimeSec()).c_str()));
    o.println(trf("Ultimo reset: %s", sysResetReason().c_str()));
    o.println(trf("Avvii totali: %lu", (unsigned long)sysBootCount()));
    o.println(trf("Ore di vita: %lu h %lu min", (unsigned long)(sysLifeSec() / 3600UL), (unsigned long)((sysLifeSec() / 60UL) % 60UL)));
  }
  else if (c == "free") {
    o.println(trf("RAM   libera %u KB su %u KB", (unsigned)(ESP.getFreeHeap() / 1024), (unsigned)(ESP.getHeapSize() / 1024)));
    o.println(trf("PSRAM libera %u KB su %u KB", (unsigned)(ESP.getFreePsram() / 1024), (unsigned)(ESP.getPsramSize() / 1024)));
    o.println(trf("RAM minima dall'avvio %u KB, blocco piu grande %u KB", (unsigned)(ESP.getMinFreeHeap() / 1024), (unsigned)(ESP.getMaxAllocHeap() / 1024)));
  }
  else if (c == "df") {
    o.println(trf("/flash  usati %u KB su %u KB", (unsigned)(LittleFS.usedBytes() / 1024), (unsigned)(LittleFS.totalBytes() / 1024)));
  }
  else if (c == "temp") o.println(trf("Temperatura CPU: %s", fmtTemp(sysCpuTemp()).c_str()));
  else if (c == "top") {
    o.println(trf("CPU %d%%  Temp %s  %u MHz", sysCpuPercent(), fmtTemp(sysCpuTemp()).c_str(), (unsigned)getCpuFrequencyMhz()));
    o.println(trf("RAM libera %u KB  PSRAM libera %u KB", (unsigned)(ESP.getFreeHeap() / 1024), (unsigned)(ESP.getFreePsram() / 1024)));
  }
  else if (c == "ps") o.print(sysTasksText());
  else if (c == "kill") {
    String err;
    if (a1.length() == 0) o.println(tr("Uso: kill <nome task>"));
    else if (sysTaskKill(a1, err)) o.println(trf("Task '%s' fermato (start %s per farlo ripartire)", a1.c_str(), a1.c_str()));
    else o.println(err);
  }
  else if (c == "start" || c == "restart") {
    String err;
    if (a1.length() == 0) o.println(tr("Uso: start <nome task>  /  restart <nome task>"));
    else if (sysTaskRestart(a1, err)) o.println(trf("Task '%s' avviato", a1.c_str()));
    else o.println(err);
  }
  else if (c == "rules") {
    String a2 = argAt(line, 2);
    if (a1 == "run") {
      String err;
      if (rulesRunNow(a2.toInt() - 1, err)) o.println(trf("Regola %d avviata", (int)a2.toInt()));
      else o.println(trf("Errore: %s", err.c_str()));
    } else {
      String t = rulesText();
      if (t.length() == 0) o.println(tr("Nessuna regola"));
      else {
        int n = 0, p = 0;
        while (p < (int)t.length()) {
          int e = t.indexOf('\n', p); if (e < 0) e = t.length();
          String l = t.substring(p, e); l.trim(); p = e + 1;
          if (!l.length() || l[0] == '#') continue;
          n++;
          o.println(String(n) + "  " + l);
        }
      }
    }
  }
  else if (c == "boot-order") {
    if (a1 == "reset") { bootResetOrder(); o.println(tr("Ordine di avvio predefinito (vale dal prossimo riavvio)")); }
    else if (a1 == "" || a1 == "lista") o.println(bootJson());
    else {
      String res, err;
      if (bootSetOrder(a1, res, err)) o.println(trf("Ordine salvato: %s (vale dal prossimo riavvio)", res.c_str()));
      else o.println(trf("Errore: %s", err.c_str()));
    }
  }
  else if (c == "ls") {
    String j = fsListJson(a1.length() ? a1 : "/");
    if (j.indexOf("\"ok\":true") < 0) { o.println(tr("Cartella non trovata")); }
    else {
      // lista semplice: scorro il JSON {"n":"..","d":bool,"s":N}
      int pos = 0;
      while ((pos = j.indexOf("{\"n\":\"", pos)) >= 0) {
        int e = j.indexOf("\",\"d\":", pos);
        String nm = j.substring(pos + 6, e);
        bool dir = j.startsWith("true", e + 7);
        int sp = j.indexOf("\"s\":", e); int se = j.indexOf('}', sp);
        o.println(String(dir ? "[D] " : "    ") + nm + (dir ? "" : "  " + j.substring(sp + 4, se) + " B"));
        pos = se;
      }
    }
  }
  else if (c == "cat") {
    if (!a1.length()) { o.println(tr("Uso: cat <file>")); return; }
    String t, err;
    if (!fsReadText(a1, t, err)) { o.println(err); return; }
    o.println(t);
  }
  else if (c == "mkdir") {
    String err; if (!a1.length()) { o.println(tr("Uso: mkdir <cartella>")); return; }
    o.println(fsMkdir(a1, err) ? String(tr("Creata")) : err);
  }
  else if (c == "rm") {
    String err; if (!a1.length()) { o.println(tr("Uso: rm <file o cartella>")); return; }
    o.println(fsRemove(a1, err) ? String(tr("Eliminato")) : err);
  }
  else if (c == "mv" || c == "cp") {
    String a2 = argAt(line, 2), err;
    if (!a1.length() || !a2.length()) { o.println(trf("Uso: %s <da> <a>", c.c_str())); return; }
    bool r = (c == "mv") ? fsRename(a1, a2, err) : fsCopy(a1, a2, err);
    o.println(r ? String(tr("Fatto")) : err);
  }
  else if (c == "write") {
    String txt = restFrom(line, 2), err;
    if (!a1.length()) { o.println(tr("Uso: write <file> <testo>")); return; }
    o.println(fsWriteText(a1, txt, err) ? String(tr("Scritto")) : err);
  }
  else if (c == "date") {
    if (a1 == "set") { String err; if (timeSetLocal(restFrom(line, 2), err)) o.println(timeNowStr()); else o.println(err); }
    else o.println(timeNowStr() + "  (" + cfg.tzName + ")");
  }
  else if (c == "ntp") {
    if (a1 == "on" || a1 == "off") { cfg.ntpOn = (a1 == "on"); cfgSave(); timeApply(); o.println(cfg.ntpOn ? tr("NTP acceso") : tr("NTP spento")); }
    else if (a1 == "sync") { timeApply(); o.println(tr("Sincronizzazione NTP richiesta")); }
    else if (a1 == "every") {
      long m = argAt(line, 2).toInt();
      if (!argAt(line, 2).length() || !timeEveryValid(m)) { o.println(tr("Uso: ntp every 0|15|60|360|720|1440|10080  (minuti, 0 = solo all'avvio)")); return; }
      cfg.ntpEvery = m; cfgSave(); timeApply(); o.println(trf("Sincronizzazione NTP ogni %ld minuti", m));
    }
    else o.println(timeJson());
  }
  else if (c == "cpu") {
    if (!a1.length()) { o.println(cfg.cpuMhz ? trf("CPU a %u MHz, modo %u fisso", (unsigned)getCpuFrequencyMhz(), (unsigned)cfg.cpuMhz) : trf("CPU a %u MHz, modo automatico", (unsigned)getCpuFrequencyMhz())); return; }
    int m = (a1 == "auto") ? 0 : a1.toInt();
    if (m != 0 && m != 80 && m != 160 && m != 240) { o.println(tr("Uso: cpu auto|80|160|240")); return; }
    cfg.cpuMhz = m; cfgSave(); sysApplyCpuMode(); o.println(trf("Modo CPU: %s", a1.c_str()));
  }
  else if (c == "wifi" && a1 == "set") {
    String ss = argAt(line, 2), pw = restFrom(line, 3);
    if (!ss.length() || ss.length() > 32) { o.println(tr("Uso: wifi set <rete> [password]")); return; }
    if (pw.length() && (pw.length() < 8 || pw.length() > 63)) { o.println(tr("Password Wi-Fi: da 8 a 63 caratteri")); return; }
    cfg.staEnabled = true; cfg.staSsid = ss; cfg.staPass = pw; cfg.staDhcp = true;
    cfgSave(); netReconfigure(); o.println(trf("Mi collego a %s: se non riesce l'hotspot torna da solo", ss.c_str()));
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
    o.println(trf("Rete: stato %d, modo Wi-Fi %d (1 client, 2 hotspot, 3 entrambi, 0 spento)", (int)netState(), (int)WiFi.getMode()));
    o.println(trf("Servizi: hotspot %s, DHCP %s, portale %s, mDNS %s", cfg.apOn ? tr("acceso") : tr("spento"), cfg.dhcpOn ? tr("acceso") : tr("spento"), netCaptive() ? tr("acceso") : tr("spento"), cfg.mdnsOn ? tr("acceso") : tr("spento")));
    o.println(trf("Web: HTTP %s porta %u, HTTPS %s porta %u (in funzione: %s)", cfg.httpOn ? tr("acceso") : tr("spento"), (unsigned)cfg.httpPort, cfg.https ? tr("acceso") : tr("spento"), (unsigned)cfg.httpsPort, webHttpsUp() ? tr("si") : tr("no")));
    o.println(trf("RAM libera %u KB, minima %u KB, blocco piu grande %u KB, PSRAM libera %u KB", (unsigned)(ESP.getFreeHeap() / 1024), (unsigned)(ESP.getMinFreeHeap() / 1024), (unsigned)(ESP.getMaxAllocHeap() / 1024), (unsigned)(ESP.getFreePsram() / 1024)));
    o.println(trf("/flash usati %u KB su %u KB", (unsigned)(LittleFS.usedBytes() / 1024), (unsigned)(LittleFS.totalBytes() / 1024)));
    o.println(trf("Blocchi grandi (TLS) in PSRAM: %s", g_extmem ? tr("attivo") : tr("non attivo")));
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
      return;
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
      if (a1 == "dhcp") { if (!v && netState() != NET_CLIENT_OK) { o.println(tr("Spegni il DHCP dell'hotspot solo quando la scheda e collegata alla Wi-Fi di casa: senza DHCP il telefono dovrebbe avere un indirizzo fisso")); return; } cfg.dhcpOn = v; }
      else cfg.mdnsOn = v;
      cfgSave(); netApplyServices(); o.println(tr("Salvato e applicato.")); return;
    }
    else { o.println(tr("Uso: svc [ap|captive|http|https|dhcp|mdns on|off] [http-port|https-port <numero>]")); return; }
    String e = cfgSvcCheck(apOn, hOn, hp, sOn, sp);
    if (e.length()) { o.println(e); return; }
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
    o.println(trf("/flash  usati %u KB su %u KB", (unsigned)(LittleFS.usedBytes() / 1024), (unsigned)(LittleFS.totalBytes() / 1024)));
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
    else { o.println(tr("Uso: led state|heartbeat|fixed|off")); return; }
    ledApplyConfig(); cfgSave(); o.println(trf("LED: %s", a1.c_str()));
  }
  else if (c == "led-color") {
    if (a1.length() != 6) { o.println(tr("Uso: led-color RRGGBB")); return; }
    cfg.ledColor = (uint32_t)strtoul(a1.c_str(), NULL, 16) & 0xFFFFFF;
    cfgSave(); o.println(trf("Colore: %s", hex6(cfg.ledColor).c_str()));
  }
  else if (c == "led-bright") {
    int v = a1.toInt();
    if (!a1.length() || v < 0 || v > 255) { o.println(tr("Uso: led-bright 0-255")); return; }
    cfg.ledBrightness = v; cfgSave(); o.println(trf("Luminosita: %d", v));
  }
  else if (c == "hostname") {
    if (!a1.length()) { o.println(cfg.hostname); return; }
    a1.toLowerCase();
    if (!hostnameValid(a1)) { o.println(tr("Nome non valido (1-32: lettere, numeri, trattino)")); return; }
    cfg.hostname = a1; cfgSave(); netReconfigure(); o.println(trf("Nome host: %s", a1.c_str()));
  }
  else if (c == "domain") {
    if (!a1.length()) { o.println(cfg.domain.length() ? cfg.domain : String(tr("(nessun dominio)"))); return; }
    if (a1 == "-") a1 = "";
    a1.toLowerCase();
    if (!domainValid(a1)) { o.println(tr("Dominio non valido")); return; }
    cfg.domain = a1; cfgSave(); o.println(a1.length() ? trf("Dominio: %s", a1.c_str()) : String(tr("Dominio tolto")));
  }
  else if (c == "lang") {
    if (!a1.length()) {
      o.println(trf("Lingua: %s", cfg.lang.c_str()));
      o.println(trf("Lingue installate: %s", langListJson().c_str()));
      return;
    }
    a1.toLowerCase();
    String err;
    if (!langSet(a1, err)) { o.println(err); return; }
    o.println(trf("Lingua: %s", cfg.lang.c_str()));
  }
  else if (c == "license" || c == "licenza") {
    String id = a1.length() ? a1 : String("notice");
    int li = licFind(id);
    if (li < 0) { o.println(trf("Licenza sconosciuta. Disponibili: %s", licIds().c_str())); return; }
    if (licZ(li)) { o.println(tr("Il testo completo delle licenze si legge dalla pagina (Sistema > Note legali) e nel file LICENSE del progetto.")); return; }
    const char* tx = licText(li); size_t n = licSize(li);
    for (size_t off = 0; off < n; off += 256) {            // a blocchi, senza copiare tutto in RAM
      size_t k = (n - off < 256) ? (n - off) : 256;
      o.write((const uint8_t*)(tx + off), k);
    }
    o.println();
  }
  else if (c == "pin") {
    String a1 = argAt(line, 1), a2 = argAt(line, 2), a3 = argAt(line, 3);
    if (a1 == "") { o.println(tr("Uso: pin <numero> [high|low|blink|read [up|down]|off]")); return; }
    int g = a1.toInt();
    if (a2 == "") {
      const char* why = pinTestBlock(g);
      o.println(why ? trf("GPIO%d: non provabile (%s)", g, why) : trf("GPIO%d: libero, si puo provare", g));
      return;
    }
    String err;
    if (!pinTestRequest(g, a2, a3, err)) { o.println(trf("Errore: %s", err.c_str())); return; }
    o.println(trf("GPIO%d: %s - si spegne da solo dopo qualche secondo", g, a2.c_str()));
  }
  else if (c == "pins") {
    for (int i = 0; i < pinCount(); i++) {
      const PinInfo* p = pinAt(i);
      o.println("GPIO" + String(p->gpio) + "  " + tr(p->owner) + "  " + tr(p->note));
    }
  }
  else if (c == "config") o.print(cfgExport(false));
  else if (c == "passwd") {
    String np = restFrom(line, 2), err;
    int u = userFind(a1);
    if (u < 0 || !np.length()) { o.println(tr("Uso: passwd <utente> <nuova password>")); return; }
    if (!userSetPassword(u, np, err)) { o.println(err); return; }
    cfgSave(); serialAuthSet(true);
    o.println(tr("Password cambiata"));
  }
  else if (c == "user" || c == "users") {
    String a2 = argAt(line, 2), a3 = argAt(line, 3), err;
    if (a1 == "" || a1 == "list") { o.print(usersText()); return; }
    if (a1 == "add") { if (!userAdd(a2, roleArg(a3) < 0 ? 255 : roleArg(a3), restFrom(line, 4), err)) { o.println(err); return; } }
    else if (a1 == "del") { if (!userDel(userFind(a2), err)) { o.println(err); return; } }
    else if (a1 == "role") { if (roleArg(a3) < 0) { o.println(tr("Ruoli: admin, oper, guest")); return; } if (!userSet(userFind(a2), roleArg(a3), -1, err)) { o.println(err); return; } }
    else if (a1 == "on" || a1 == "off") { if (!userSet(userFind(a2), -1, a1 == "on" ? 1 : 0, err)) { o.println(err); return; } }
    else { o.println(tr("Uso: user [add <nome> <admin|oper|guest> <password>|del <nome>|role <nome> <ruolo>|on <nome>|off <nome>]")); return; }
    cfgSave(); o.print(usersText());
  }
  else if (c == "ap") {
    if (a1 == "new") { cfg.apPass = cfgNewApPass(); cfgSave(); netReconfigure(); }
    else if (a1 != "" && a1 != "show") { o.println(tr("Uso: ap [show|new]")); return; }
    o.println(trf("Hotspot: %s  password: %s", cfg.apSsid.c_str(), cfg.apPass.c_str()));
  }
  else if (c == "locale") {
    String a2 = argAt(line, 2), a3 = argAt(line, 3);
    if (a1 == "country") { a2.toUpperCase(); if (!regionValid(a2)) { o.println(tr("Paese sconosciuto")); return; } cfg.country = a2; cfgSave(); regionApplyRadio(); }
    else if (a1 == "tx") { int t = a2.toInt(); if (t < 0 || t > 20) { o.println(tr("Potenza: da 2 a 20 dBm (0 = massima consentita)")); return; } cfg.txDbm = t; cfgSave(); regionApplyRadio(); }
    else if (a1 == "antenna") { if (a2 == "int") { cfg.antExt = 0; } else if (a2 == "ext") { int g = a3.toInt(); if (g < 0 || g > 15) { o.println(tr("Guadagno dell'antenna: da 0 a 15 dBi")); return; } cfg.antExt = 1; cfg.antGain = g; } else { o.println(tr("Uso: locale antenna int | locale antenna ext <dBi>")); return; } cfgSave(); regionApplyRadio(); }
    else if (a1 != "") { o.println(tr("Uso: locale [country XX|tx <dBm>|antenna int|ext <dBi>]")); return; }
    o.print(regionText());
  }
  else if (c == "firewall") {
    String a2 = argAt(line, 2), err;
    if (a1 == "off") { fwOff("seriale"); }
    else if (a1 == "confirm") fwConfirm();
    else if (a1 == "mode") { int m = a2.toInt(); if (a2 == "" || m < 0 || m > 3) { o.println(tr("Modi: 0 spento, 1 solo la mia rete, 2 lista consentita, 3 lista bloccati")); return; } fwTryStart(0); cfg.fwMode = m; cfgSave(); }
    else if (a1 == "add") { uint32_t a, b; if (cfg.fwN >= VOS_FW_MAX || !cfgParseRange(a2, a, b)) { o.println(tr("Indirizzo non valido o lista piena")); return; } cfg.fw[cfg.fwN].a = a; cfg.fw[cfg.fwN].b = b; cfg.fw[cfg.fwN].on = true; cfg.fw[cfg.fwN].name = cleanAscii(restFrom(line, 3)); cfg.fwN++; cfgSave(); }
    else if (a1 == "del") { int n = a2.toInt(); if (n < 1 || n > cfg.fwN) { o.println(tr("Voce non trovata")); return; } for (int i = n - 1; i < cfg.fwN - 1; i++) cfg.fw[i] = cfg.fw[i + 1]; cfg.fwN--; cfgSave(); }
    else if (a1 == "test") { uint32_t ip; if (!ipParse(a2, ip)) { o.println(tr("IP non valido")); return; } o.println(fwAllow(ip, false) ? tr("ammesso") : tr("rifiutato")); return; }
    else if (a1 != "" && a1 != "status") { o.println(tr("Uso: firewall [off|confirm|mode <0-3>|add <ip|a-b|rete/nn> [nome]|del <n>|test <ip>]")); return; }
    o.print(fwText());
  }
  else if (c == "audit") {
    if (a1 == "ack") { auditAck(argAt(line, 2).toInt()); }
    o.print(auditText());
  }
  else if (c == "watchdog") o.print(wdText());
  else if (c == "mesh") {
    String a2 = argAt(line, 2), err;
    if (a1 == "start") { if (!meshStart(err)) { o.println(err); return; } }
    else if (a1 == "stop") meshStop();
    else if (a1 == "send") { if (!meshSendText(a2, restFrom(line, 3), err)) { o.println(err); return; } o.println(tr("Inviato")); return; }
    else if (a1 == "cmd") { if (!meshSendCmd(a2, restFrom(line, 3), err)) { o.println(err); return; } o.println(tr("Inviato")); return; }
    else if (a1 == "key") { if (a2 == "new") { cfg.meshKey = meshNewKey(); cfgSave(); } o.println(cfg.meshKey.length() ? cfg.meshKey : String(tr("(nessuna chiave)"))); return; }
    else if (a1 == "role") { int r = a2.toInt(); if (a2 == "" || r < 0 || r > 2) { o.println(tr("Ruoli: 0 nodo, 1 gateway, 2 sensore")); return; } cfg.meshRole = r; cfgSave(); }
    else if (a1 != "" && a1 != "status") { o.println(tr("Uso: mesh [start|stop|send <scheda|*> <testo>|cmd <scheda> <azione>|key new|role <0|1|2>]")); return; }
    o.print(meshText());
  }
  else if (c == "ble") {
    String err;
    if (a1 == "on") { if (!bleStart(err)) { o.println(err); return; } }
    else if (a1 == "off") bleStop();
    o.print(bleText());
  }
  else if (c == "cert") {
    if (a1 == "new") { tlsRegenerate(); o.println(tr("Nuovo certificato pronto: vale dal prossimo riavvio (reboot)")); return; }
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
  else if (c == "welcome") shellWelcome(o, !cfg.setupDone && &o == &Serial);
  else if (c == "log") { if (a1 == "clear") { logClear(); o.println(tr("Registro svuotato")); } else o.print(logGet(a1.length() ? constrain((int)a1.toInt(), 1, 150) : 30)); }
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
    if (a1 == "start" || a1 == "restart") { if (!cfg.mqttHost.length()) { o.println(tr("Manca l'indirizzo del broker")); return; } mqttStart(); o.println(tr("MQTT avviato")); }
    else if (a1 == "stop") { mqttStop(); o.println(tr("MQTT fermato")); }
    else if (a1 == "pub") {
      String tp = argAt(line, 2), msg = restFrom(line, 3);
      if (!tp.length()) { o.println(tr("Uso: mqtt pub <argomento> <testo>")); return; }
      o.println(mqttPublishRel(tp, msg) ? tr("Inviato") : tr("MQTT non collegato"));
    }
    else if (a1 == "" || a1 == "status") o.print(mqttStatusText());
    else o.println(tr("Uso: mqtt [status|start|stop|restart|pub <argomento> <testo>]"));
  }
  else if (c == "ban") o.print(authBanText());
  else if (c == "unban") {
    if (!a1.length()) { o.println(tr("Uso: unban <indirizzo IP> | unban all")); return; }
    o.println(authUnban(a1) ? tr("Sbloccato") : tr("Indirizzo non trovato"));
  }
  else if (c == "sleep") {
    int m = a1.toInt();
    if (a1.length() && (m < 1 || m > 10080)) { o.println(tr("Sonno: da 1 minuto a 7 giorni")); return; }
    if (m) { o.println(trf("Sonno profondo per %d minuti: poi la scheda riparte da sola (o con RESET)", m)); delay(300); powerSleepNow((uint32_t)m * 60UL); }
    else { o.println(tr("Sonno profondo: si riaccende con il tasto RESET")); delay(300); sysSleep(); }
  }
  else if (c == "selftest") {
    String e;
    bool act = line.indexOf("active") >= 0, nm = line.indexOf("names") >= 0;
    if (!selftestStart(act, nm, e)) { o.println(e); return; }
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
    else if (a1 != "" && a1 != "status") { o.println(tr("Uso: stats [on|off|reset]")); return; }
    o.print(statsText());
  }
  else if (c == "power") {
    String e;
    if (a1 == "off") powerSet(0, 0, 0, e);
    else if (a1 == "wifi") powerSet(1, 0, 0, e);
    else if (a1 == "cycle") powerSet(2, (uint32_t)argAt(line, 2).toInt(), (uint32_t)argAt(line, 3).toInt(), e);
    else if (a1 != "" && a1 != "status") { o.println(tr("Uso: power [off|wifi|cycle <minuti sveglia> <minuti sonno>]")); return; }
    if (e.length()) { o.println(e); return; }
    if (a1 != "" && a1 != "status") { cfgSave(); o.println(tr("Salvato e applicato.")); }
    o.print(powerText());
  }
  else if (c == "reboot") { o.println(tr("Riavvio...")); delay(300); ESP.restart(); }
  else o.println(tr("Comando sconosciuto. Scrivi 'help'."));
}

// ---------- schermata di benvenuto (solo ASCII, 68 colonne) ----------
#define WW 68
static void wBar(Print& o) { String b = "+"; for (int i = 0; i < WW - 2; i++) b += '-'; o.println(b + "+"); }
static void wLine(Print& o, const String& t) {
  String x = t; if (x.length() > WW - 4) x = x.substring(0, WW - 4);
  String l = "|  " + x; while (l.length() < WW - 1) l += ' ';
  o.println(l + "|");
}
// testo lungo: a capo sulle parole
static void wPara(Print& o, const String& t) {
  String rest = t;
  while (rest.length()) {
    if ((int)rest.length() <= WW - 4) { wLine(o, rest); break; }
    int cut = rest.lastIndexOf(' ', WW - 4); if (cut <= 0) cut = WW - 4;
    wLine(o, rest.substring(0, cut)); rest = rest.substring(cut + 1);
  }
}

static String g_lcodes[9], g_lnames[9];
static int g_ln = 0;

static void langMenu(Print& o) {
  g_ln = langList(g_lcodes, g_lnames, 9);
  String l = String(tr("Lingua:")) + " ";
  for (int i = 0; i < g_ln; i++) l += " [" + String(i + 1) + "] " + g_lnames[i] + (g_lcodes[i] == cfg.lang ? " *" : "") + " ";
  l += String(" ") + tr("(premi il numero)");
  wPara(o, l);
}

void shellWelcome(Print& o, bool full) {
  o.println();
  wBar(o);
  { String l = "|   /\\      " + String(VOS_NAME) + " " + VOS_VERSION; while (l.length() < WW - 1) l += ' '; o.println(l + "|"); }
  { String l = "|  /  \\     " + String(VOS_BOARD) + " - 4 MB flash, 2 MB PSRAM"; while (l.length() < WW - 1) l += ' '; o.println(l + "|"); }
  { String l = "| /____\\    github.com/paolellad79/VesevOS"; while (l.length() < WW - 1) l += ' '; o.println(l + "|"); }
  wBar(o);
  if (full) {
    wLine(o, tr("BENVENUTO! Questa scheda non e ancora configurata."));
    wLine(o, "");
    wLine(o, tr("1. Collegati alla rete Wi-Fi:"));
    wLine(o, String("     ") + tr("Nome:") + "      " + cfg.apSsid);
    wLine(o, String("     ") + tr("Password:") + "  " + cfg.apPass);
    wLine(o, String(tr("2. Apri:")) + "      http://192.168.4.1");
    wLine(o, String("   ") + tr("(di solito la pagina si apre da sola)"));
    wLine(o, tr("3. Segui la configurazione guidata."));
    wLine(o, "");
    wLine(o, tr("Il QR per il telefono e nella pagina Home."));
    wBar(o);
    wPara(o, tr("Perche una password diversa per ogni scheda? Lo chiedono le leggi sulla sicurezza dei dispositivi: UE (RED/EN 18031, Cyber Resilience Act) e Regno Unito (PSTI). Niente password uguali."));
    wBar(o);
    wLine(o, tr("Password persa?  Tieni premuto BOOT 8 s  -> la riscrivo qui"));
    wLine(o, tr("Da capo?         Tieni premuto BOOT 20 s -> fabbrica"));
    wLine(o, tr("Aiuto: http://192.168.4.1/#aiuto   Comandi: scrivi  help"));
  } else {
    wLine(o, String(VOS_NAME) + " " + VOS_VERSION + "  -  " + cfg.hostname + " (" + cfg.hostname + ".local)");
    if (netState() == NET_CLIENT_OK) wLine(o, trf("Wi-Fi casa: %s  %s  %d dBm", WiFi.SSID().c_str(), netIpString().c_str(), (int)WiFi.RSSI()));
    else wLine(o, trf("Hotspot: %s  %s", cfg.apSsid.c_str(), netIpString().c_str()));
    wLine(o, trf("Paese: %s   Ora: %s", cfg.country.length() ? cfg.country.c_str() : "--", timeNowStr().c_str()));
    int red = auditCount(AUD_RED), yel = auditCount(AUD_YELLOW);
    wLine(o, (red || yel) ? trf("Allarmi: %d rossi, %d gialli -> scrivi audit", red, yel) : String(tr("Allarmi: nessuno")));
    wLine(o, trf("Avvio: %s (n. %lu)", sysResetReason().c_str(), (unsigned long)sysBootCount()));
    if (cfg.https && tlsFingerprint().length()) wPara(o, String(tr("Impronta HTTPS:")) + " " + tlsFingerprint().substring(0, 47) + "...");
    wLine(o, trf("Pagina: https://%s   Comandi: help", netIpString().c_str()));
  }
  wBar(o);
  langMenu(o);
  wBar(o);
}

// ---------- shell seriale ----------
static String g_buf;
static bool g_fullWelcome = false;

// La seriale chiede la password solo se: password impostata, interruttore acceso, non ancora autenticata
static bool serialLocked() { return authIsSet() && cfg.serialAuth && !serialAuthed(); }

static void prompt() { Serial.print(serialLocked() ? "password: " : "vesevos> "); }

void shellSerialShowPass() { g_fullWelcome = true; }   // tasto BOOT 8 s: rimostra la password dell'hotspot

// ---- configurazione guidata da seriale (stessi passi della pagina web) ----
enum { WZ_OFF = -1, WZ_LANG = 0, WZ_COUNTRY, WZ_NAME, WZ_ANT, WZ_ANTG, WZ_AP, WZ_PASS, WZ_WIFI, WZ_SCAN, WZ_WPASS, WZ_TIME };
static int g_wz = WZ_OFF;
static String g_wzNets[8];
static int g_wzN = 0;
static String g_wzSsid, g_wzPass;
static bool g_wzWifi = false;

static void wzAsk(Print& o) {
  switch (g_wz) {
    case WZ_LANG: o.println(trf("1/8 Lingua [%s] (it, en, ...):", cfg.lang.c_str())); break;
    case WZ_COUNTRY: o.println(trf("2/8 Paese, codice di 2 lettere (es. IT) [%s]:", cfg.country.length() ? cfg.country.c_str() : "-")); break;
    case WZ_NAME: o.println(trf("3/8 Nome della scheda [%s]:", cfg.hostname.c_str())); break;
    case WZ_ANT: o.println(tr("4/8 Antenna: i = interna, e = esterna [i]:")); break;
    case WZ_ANTG: o.println(tr("Guadagno dell'antenna esterna in dBi (0-15) [0]:")); break;
    case WZ_AP: o.println(trf("5/8 Hotspot: %s  password: %s", cfg.apSsid.c_str(), cfg.apPass.c_str())); o.println(tr("Invio = tieni, n = nuova password casuale:")); break;
    case WZ_PASS:
      if (authIsSet()) o.println(tr("6/8 Amministratore gia impostato. Invio = tieni, oppure scrivi la nuova password:"));
      else o.println(trf("6/8 Scegli la password dell'amministratore '%s' (almeno 6 caratteri; si vede mentre scrivi: fallo in un luogo sicuro):", cfg.users[authFirstAdmin() < 0 ? 0 : authFirstAdmin()].name.c_str()));
      break;
    case WZ_WIFI: o.println(tr("7/8 Collegare la Wi-Fi di casa? s = si, Invio = no (hotspot e HTTP restano accesi):")); break;
    case WZ_SCAN: o.println(tr("Numero della rete, oppure scrivi il nome:")); break;
    case WZ_WPASS: o.println(tr("Password della Wi-Fi (Invio = rete aperta):")); break;
    case WZ_TIME: o.println(tr("8/8 Ora a mano, AAAA-MM-GG HH:MM (Invio = salta):")); break;
  }
}

static void wzNext(Print& o);
static void wzFinish(Print& o) {
  g_wz = WZ_OFF;
  cfg.ntpOn = g_wzWifi;                       // senza Wi-Fi di casa il server dell'ora non serve
  if (g_wzWifi) { cfg.staEnabled = true; cfg.staSsid = g_wzSsid; cfg.staPass = g_wzPass; cfg.staDhcp = true; }
  cfg.setupDone = true; ledSetSetup(false);
  cfgSetOrigin("seriale setup"); cfgSave(); timeApply();
  o.println(tr("Configurazione finita."));
  o.println(trf("Nome: %s.local   Paese: %s   Lingua: %s", cfg.hostname.c_str(), cfg.country.c_str(), cfg.lang.c_str()));
  if (g_wzWifi) { o.println(trf("Mi collego a %s: se non riesce l'hotspot torna da solo.", g_wzSsid.c_str())); netReconfigure(); }
  else o.println(tr("Senza Wi-Fi di casa: hotspot e HTTP restano accesi, il server dell'ora e spento."));
  o.println(tr("Per i servizi di rete e le porte: svc. Per tutto il resto: help."));
}

static void wzNext(Print& o) {
  switch (g_wz) {
    case WZ_LANG: g_wz = WZ_COUNTRY; break;
    case WZ_COUNTRY: g_wz = WZ_NAME; break;
    case WZ_NAME: g_wz = WZ_ANT; break;
    case WZ_ANT: g_wz = WZ_AP; break;
    case WZ_ANTG: g_wz = WZ_AP; break;
    case WZ_AP: g_wz = WZ_PASS; break;
    case WZ_PASS: g_wz = WZ_WIFI; break;
    case WZ_WIFI: g_wz = g_wzWifi ? WZ_SCAN : WZ_TIME; break;
    case WZ_SCAN: g_wz = WZ_WPASS; break;
    case WZ_WPASS: g_wz = WZ_TIME; break;
    case WZ_TIME: wzFinish(o); return;
  }
  if (g_wz == WZ_SCAN) {                      // elenco reti
    netScanStart(); delay(300);
    for (int i = 0; i < 30 && netScanJson().indexOf("\"running\":true") >= 0; i++) delay(300);
    String j = netScanJson(); g_wzN = 0;
    int p = 0;
    while (g_wzN < 8) {
      int a = j.indexOf("\"ssid\":\"", p); if (a < 0) break;
      a += 8; int b = j.indexOf("\",\"rssi\"", a); if (b < 0) break;
      String sd = j.substring(a, b); p = b;
      if (sd.length() && sd.indexOf('\\') < 0) { bool dup = false; for (int k = 0; k < g_wzN; k++) if (g_wzNets[k] == sd) dup = true; if (!dup) g_wzNets[g_wzN++] = sd; }
    }
    for (int i = 0; i < g_wzN; i++) o.println(String(i + 1) + ") " + g_wzNets[i]);
    if (!g_wzN) o.println(tr("Nessuna rete trovata: scrivi il nome a mano."));
  }
  wzAsk(o);
}

static void wzStart(Print& o) {
  if (&o != &Serial) { o.println(tr("La configurazione guidata si fa solo dalla seriale (USB).")); return; }
  g_wz = WZ_LANG; g_wzWifi = false; g_wzSsid = ""; g_wzPass = "";
  o.println(tr("Configurazione guidata da seriale. Invio = tieni il valore proposto, skip = salta il passo, quit = esci."));
  wzAsk(o);
}

static void wzLine(Print& o, String l) {
  l.trim();
  if (l == "quit") { g_wz = WZ_OFF; o.println(tr("Configurazione interrotta: la guida resta da finire (scrivi setup per riprendere).")); return; }
  String err;
  bool skip = (l == "skip" || l.length() == 0);
  switch (g_wz) {
    case WZ_LANG: if (!skip) { l.toLowerCase(); if (!langSet(l, err)) { o.println(err); wzAsk(o); return; } } break;
    case WZ_COUNTRY: if (!skip) { l.toUpperCase(); if (!regionValid(l)) { o.println(tr("Paese sconosciuto")); wzAsk(o); return; } cfg.country = l; cfgSave(); regionApplyRadio(); } break;
    case WZ_NAME: if (!skip) { l.toLowerCase(); if (!hostnameValid(l)) { o.println(tr("Nome non valido (1-32: lettere, numeri, trattino)")); wzAsk(o); return; } cfg.hostname = l; cfgSave(); } break;
    case WZ_ANT:
      if (l == "e") { cfg.antExt = 1; g_wz = WZ_ANTG; wzAsk(o); return; }
      if (!skip && l != "i") { o.println(tr("Scrivi i oppure e")); wzAsk(o); return; }
      cfg.antExt = 0; cfg.antGain = 0; cfgSave(); regionApplyRadio(); break;
    case WZ_ANTG: { int g = skip ? 0 : l.toInt(); if (g < 0 || g > 15) { o.println(tr("Guadagno dell'antenna: da 0 a 15 dBi")); wzAsk(o); return; } cfg.antGain = g; cfgSave(); regionApplyRadio(); } break;
    case WZ_AP: if (l == "n") { cfg.apPass = cfgNewApPass(); cfgSave(); o.println(trf("Nuova password hotspot: %s", cfg.apPass.c_str())); } break;
    case WZ_PASS:
      if (!skip) {
        int a = authFirstAdmin();
        if (a < 0 || !userSetPassword(a, l, err)) { o.println(err.length() ? err : String(tr("Errore"))); wzAsk(o); return; }
        cfg.users[a].on = true; cfg.users[a].role = ROLE_ADMIN; cfgSave();
      } else if (!authIsSet()) { o.println(tr("Serve una password per l'amministratore.")); wzAsk(o); return; }
      break;
    case WZ_WIFI: g_wzWifi = (l == "s" || l == "S" || l == "si"); break;
    case WZ_SCAN: {
      if (skip) { g_wzWifi = false; g_wz = WZ_TIME; wzAsk(o); return; }
      int n = l.toInt();
      g_wzSsid = (n >= 1 && n <= g_wzN && String(n) == l) ? g_wzNets[n - 1] : l;
      if (g_wzSsid.length() > 32) { o.println(tr("Nome rete non valido")); wzAsk(o); return; }
    } break;
    case WZ_WPASS:
      if (l.length() && (l.length() < 8 || l.length() > 63)) { o.println(tr("Password Wi-Fi: da 8 a 63 caratteri")); wzAsk(o); return; }
      g_wzPass = l; break;
    case WZ_TIME:
      if (!skip) { if (!timeSetLocal(l, err)) { o.println(err); wzAsk(o); return; } o.println(timeNowStr()); }
      break;
  }
  wzNext(o);
}

bool shellWizardActive() { return g_wz != WZ_OFF; }

void shellSerialPoll() {
  static bool first = true, wasOpen = false;
  static uint32_t lastBanner = 0, lastKey = 0;
  uint32_t now = millis();
  bool open = (bool)Serial;                       // true quando il monitor seriale e aperto sul PC
  bool show = first || g_fullWelcome;
  if (!cfg.setupDone && open) {
    // finche la configurazione guidata non e finita il benvenuto (con la password dell'hotspot) NON sparisce:
    // si rimostra a ogni apertura del monitor e, se nessuno scrive, ogni 30 secondi
    if (!wasOpen) show = true;
    else if (now - lastBanner >= 30000UL && now - lastKey >= 30000UL) show = true;
  }
  wasOpen = open;
  if (show) {
    bool full = !cfg.setupDone || g_fullWelcome;
    first = false; g_fullWelcome = false; lastBanner = now;
    shellWelcome(Serial, full);
    if (!authIsSet()) Serial.println(tr("Nessuna password: impostala dalla pagina web (primo accesso)."));
    if (!cfg.setupDone) { Serial.println(tr("Configurazione non finita: questo messaggio resta finche la guida non e completata (pagina web oppure seriale).")); Serial.println(tr("Per configurare da questa seriale scrivi: setup")); }
    prompt();
  }
  while (Serial.available()) {
    char ch = Serial.read();
    lastKey = now;
    // tasto numerico a riga vuota: cambia lingua (solo con la seriale sbloccata)
    if (!g_buf.length() && ch >= '1' && ch <= '9' && !serialLocked() && g_wz == WZ_OFF) {
      int i = ch - '1';
      if (!g_ln) g_ln = langList(g_lcodes, g_lnames, 9);
      if (i < g_ln) {
        String err;
        if (langSet(g_lcodes[i], err)) { Serial.println(); Serial.println(trf("Lingua impostata: %s", g_lnames[i].c_str())); shellWelcome(Serial, !cfg.setupDone); }
        else Serial.println(err);
        prompt();
        continue;
      }
    }
    if (ch == '\r' || ch == '\n') {
      String l = g_buf; g_buf = "";
      if (l.length() == 0 && ch == '\n') continue;
      Serial.println();
      if (g_wz != WZ_OFF && !serialLocked()) { cfgSetOrigin("seriale"); wzLine(Serial, l); prompt(); continue; }   // risposta alla guida
      if (l.length() == 0 && !cfg.setupDone) { g_fullWelcome = true; continue; }   // Invio a vuoto: rimostra il benvenuto
      if (serialLocked()) {
        if (authLocked()) Serial.println(tr("Bloccato per troppi errori, riprova tra un minuto."));
        else if (authCheck(l)) { serialAuthSet(true); Serial.println(tr("Accesso eseguito.")); }
        else Serial.println(tr("Password errata."));
      } else if (l == "logout") { serialAuthSet(false); Serial.println(tr("Uscito.")); }
      else { cfgSetOrigin("seriale"); shellExec(l, Serial, ROLE_ADMIN); }
      prompt();
    } else if (ch == 8 || ch == 127) { if (g_buf.length()) g_buf.remove(g_buf.length() - 1); }
    else if (ch >= 32 && ch < 127 && g_buf.length() < 200) {
      g_buf += ch;
      if (!serialLocked()) Serial.print(ch);   // niente eco della password
    }
  }
}
