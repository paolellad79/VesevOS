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
#include "vos_led.h"
#include "vos_pins.h"
#include "vos_util.h"
#include "vos_files.h"
#include "vos_time.h"
#include "vos_log.h"
#include "vos_i18n.h"
#include "vos_license.h"
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
  o.println(tr("  rules [run <n>] automazioni: elenco, oppure esegui la regola n"));
  o.println(tr("  boot-order [lista|reset]  ordine di avvio dei servizi"));
  o.println(tr("  ls [cartella]   elenco file (flash)"));
  o.println(tr("  cat <file>      mostra un file"));
  o.println(tr("  mkdir <nome>    crea cartella"));
  o.println(tr("  rm <nome>       elimina file o cartella"));
  o.println(tr("  mv <da> <a>     sposta o rinomina"));
  o.println(tr("  cp <da> <a>     copia un file"));
  o.println(tr("  write <file> <testo>   scrive un file di testo"));
  o.println(tr("  date            data e ora"));
  o.println(tr("  ntp [sync]      stato NTP / risincronizza"));
  o.println(tr("  cpu [auto|80|160|240]  velocita CPU"));
  o.println(tr("  hostname [nome] mostra o cambia il nome host"));
  o.println(tr("  domain [nome|-] mostra, cambia o toglie il dominio"));
  o.println(tr("  ip              rete e indirizzo"));
  o.println(tr("  wifi            stato wi-fi"));
  o.println(tr("  wifi-scan       cerca reti"));
  o.println(tr("  led <modo>      state|heartbeat|fixed|off"));
  o.println(tr("  led-color <RRGGBB>   colore (esadecimale)"));
  o.println(tr("  led-bright <0-255>   luminosita"));
  o.println(tr("  led2 <modo>     LED aggiuntivo: off|on|heartbeat"));
  o.println(tr("  led2-pin <n>    pin del LED aggiuntivo"));
  o.println(tr("  led2-bright <0-255>  luminosita LED aggiuntivo"));
  o.println(tr("  led2-invert on|off   on = si accende con livello basso"));
  o.println(tr("  lang [codice]   mostra o cambia la lingua (it, en, ...)"));
  o.println(tr("  license [id]    note legali e licenze (notice, gpl3, lgpl3, lgpl21, apache2)"));
  o.println(tr("  pins            pin usati"));
  o.println(tr("  pin <n> [high|low|blink|read [up|down]|off]  prova un pin (si spegne da solo)"));
  o.println(tr("  config          mostra configurazione (senza password)"));
  o.println(tr("  passwd <nuova>  cambia password (min 6 caratteri)"));
  o.println(tr("  logout          esce (solo seriale)"));
  o.println(tr("  log             ultime righe di log"));
  o.println(tr("  factory-reset   azzera tutto (poi riavvia)"));
  o.println(tr("  reboot          riavvia"));
}

static String hex6(uint32_t c) {
  char b[8]; snprintf(b, sizeof(b), "%06lX", (unsigned long)(c & 0xFFFFFF)); return String(b);
}

void shellExec(const String& lineIn, Print& o, bool authed) {
  String line = lineIn; line.trim();
  if (line.length() == 0) return;
  String c = argAt(line, 0);
  c.toLowerCase();
  String a1 = argAt(line, 1);

  if (!authed && authIsSet()) { o.println(tr("Non autorizzato. Fai login.")); return; }

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
    else if (sysTaskKill(a1, err)) o.println(trf("Task '%s' fermato (riparte al riavvio)", a1.c_str()));
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
  else if (c == "date") o.println(timeNowStr() + "  (" + cfg.tzName + ")");
  else if (c == "ntp") {
    if (a1 == "sync") { timeApply(); o.println(tr("Sincronizzazione NTP richiesta")); }
    else o.println(timeJson());
  }
  else if (c == "cpu") {
    if (!a1.length()) { o.println(cfg.cpuMhz ? trf("CPU a %u MHz, modo %u fisso", (unsigned)getCpuFrequencyMhz(), (unsigned)cfg.cpuMhz) : trf("CPU a %u MHz, modo automatico", (unsigned)getCpuFrequencyMhz())); return; }
    int m = (a1 == "auto") ? 0 : a1.toInt();
    if (m != 0 && m != 80 && m != 160 && m != 240) { o.println(tr("Uso: cpu auto|80|160|240")); return; }
    cfg.cpuMhz = m; cfgSave(); sysApplyCpuMode(); o.println(trf("Modo CPU: %s", a1.c_str()));
  }
  else if (c == "ip" || c == "wifi") o.println(netStatusJson());
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
  else if (c == "led2") {
    if (a1 == "off") cfg.led2Mode = 0;
    else if (a1 == "on") cfg.led2Mode = 1;
    else if (a1 == "heartbeat") cfg.led2Mode = 2;
    else {
      const char* md[] = {"off", "on", "heartbeat"};
      o.println(trf("Stato: %s, pin %u, luminosita %u, livello %s", md[cfg.led2Mode < 3 ? cfg.led2Mode : 0], (unsigned)cfg.led2Pin, (unsigned)cfg.led2Bright, cfg.led2Invert ? tr("BASSO (invertito)") : tr("alto (normale)")));
      o.println(tr("Uso: led2 off|on|heartbeat   (led2-pin <n>, led2-bright <0-255>, led2-invert on|off)"));
      return;
    }
    ledApplyConfig(); cfgSave(); o.println(trf("LED aggiuntivo: %s (pin %u)", a1.c_str(), (unsigned)cfg.led2Pin));
  }
  else if (c == "led2-invert") {
    if (a1 != "on" && a1 != "off") { o.println(tr("Uso: led2-invert on|off   (on = si accende con livello basso)")); return; }
    cfg.led2Invert = (a1 == "on"); ledApplyConfig(); cfgSave();
    o.println(trf("Livello LED aggiuntivo: %s", cfg.led2Invert ? tr("BASSO (invertito)") : tr("alto (normale)")));
  }
  else if (c == "led2-pin") {
    int p = a1.toInt();
    if (!a1.length() || !led2PinAllowed(p)) { o.println(tr("Pin non ammesso (usa 1-18, 21 oppure 38-47)")); return; }
    if (p != cfg.led2Pin && pinIsUsed(p)) { o.println(tr("Pin gia usato da altro")); return; }
    cfg.led2Pin = p; ledApplyConfig(); cfgSave(); o.println(trf("Pin LED aggiuntivo: %d", p));
  }
  else if (c == "led2-bright") {
    int v = a1.toInt();
    if (!a1.length() || v < 0 || v > 255) { o.println(tr("Uso: led2-bright 0-255")); return; }
    cfg.led2Bright = v; cfgSave(); o.println(trf("Luminosita LED aggiuntivo: %d", v));
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
    if (li != 0 && &o != &Serial) { o.println(tr("Il testo completo e troppo lungo per la shell web: usa la scheda Config > Licenze o la seriale.")); return; }
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
    String np = restFrom(line, 1);
    if (np.length() < 6) { o.println(tr("Password troppo corta (min 6)")); return; }
    authSetPassword(np); cfgSave();
    serialAuthSet(true);
    o.println(tr("Password cambiata"));
  }
  else if (c == "log") o.print(logGet(30));
  else if (c == "factory-reset") {
    o.println(tr("Azzero tutto e riavvio..."));
    cfgFactoryReset(); delay(300); ESP.restart();
  }
  else if (c == "reboot") { o.println(tr("Riavvio...")); delay(300); ESP.restart(); }
  else o.println(tr("Comando sconosciuto. Scrivi 'help'."));
}

// ---------- shell seriale ----------
static String g_buf;
static bool g_waitPass = false;
static bool g_waitNew = false;

// La seriale chiede la password solo se: password impostata, interruttore acceso, non ancora autenticata
static bool serialLocked() { return authIsSet() && cfg.serialAuth && !serialAuthed(); }

static void prompt() { Serial.print(serialLocked() ? "password: " : "vesevos> "); }

void shellSerialPoll() {
  static bool first = true;
  if (first) { first = false; Serial.println(); Serial.println(String(VOS_NAME) + " " + VOS_VERSION + " - " + tr("scrivi 'help'"));
    Serial.println(String("GitHub: ") + VOS_GITHUB);
    if (!authIsSet()) Serial.println(tr("Nessuna password: impostala dalla pagina web (primo accesso).")); prompt(); }
  while (Serial.available()) {
    char ch = Serial.read();
    if (ch == '\r' || ch == '\n') {
      String l = g_buf; g_buf = "";
      if (l.length() == 0 && ch == '\n') continue;
      Serial.println();
      if (serialLocked()) {
        if (authLocked()) Serial.println(tr("Bloccato per troppi errori, riprova tra un minuto."));
        else if (authCheck(l)) { serialAuthSet(true); Serial.println(tr("Accesso eseguito.")); }
        else Serial.println(tr("Password errata."));
      } else if (l == "logout") { serialAuthSet(false); Serial.println(tr("Uscito.")); }
      else shellExec(l, Serial, true);
      prompt();
    } else if (ch == 8 || ch == 127) { if (g_buf.length()) g_buf.remove(g_buf.length() - 1); }
    else if (ch >= 32 && ch < 127 && g_buf.length() < 200) {
      g_buf += ch;
      if (!serialLocked()) Serial.print(ch);   // niente eco della password
    }
  }
}
