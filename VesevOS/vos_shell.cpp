// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// VesevOS - vos_shell.cpp
#include "vos_shell.h"
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
  o.println("Comandi:");
  o.println("  help            questo elenco");
  o.println("  uname           versione sistema");
  o.println("  uptime          da quanto e acceso (+ motivo reset, avvii)");
  o.println("  free            memoria RAM/PSRAM");
  o.println("  df              spazio su flash");
  o.println("  temp            temperatura CPU");
  o.println("  top             CPU, RAM, temperatura");
  o.println("  ps              elenco task");
  o.println("  ls [cartella]   elenco file (flash)");
  o.println("  cat <file>      mostra un file");
  o.println("  mkdir <nome>    crea cartella");
  o.println("  rm <nome>       elimina file o cartella");
  o.println("  mv <da> <a>     sposta o rinomina");
  o.println("  cp <da> <a>     copia un file");
  o.println("  write <file> <testo>   scrive un file di testo");
  o.println("  date            data e ora");
  o.println("  ntp [sync]      stato NTP / risincronizza");
  o.println("  cpu [auto|80|160|240]  velocita CPU");
  o.println("  hostname [nome] mostra o cambia il nome host");
  o.println("  domain [nome|-] mostra, cambia o toglie il dominio");
  o.println("  ip              rete e indirizzo");
  o.println("  wifi            stato wi-fi");
  o.println("  wifi-scan       cerca reti");
  o.println("  led <modo>      state|heartbeat|fixed|off");
  o.println("  led-color <RRGGBB>   colore (esadecimale)");
  o.println("  led-bright <0-255>   luminosita");
  o.println("  led2 <modo>     LED aggiuntivo: off|on|heartbeat");
  o.println("  led2-pin <n>    pin del LED aggiuntivo");
  o.println("  led2-bright <0-255>  luminosita LED aggiuntivo");
  o.println("  led2-invert on|off   on = si accende con livello basso");
  o.println("  pins            pin usati");
  o.println("  config          mostra configurazione (senza password)");
  o.println("  passwd <nuova>  cambia password (min 6 caratteri)");
  o.println("  logout          esce (solo seriale)");
  o.println("  log             ultime righe di log");
  o.println("  factory-reset   azzera tutto (poi riavvia)");
  o.println("  reboot          riavvia");
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

  if (!authed && authIsSet()) { o.println("Non autorizzato. Fai login."); return; }

  if (c == "help" || c == "?") cmdHelp(o);
  else if (c == "uname") o.println(String(VOS_NAME) + " " + VOS_VERSION + " (ESP32-S3, core " + ESP.getSdkVersion() + ")");
  else if (c == "uptime") {
    o.println("Acceso da: " + uptimeStr(sysUptimeSec()));
    o.println("Ultimo reset: " + sysResetReason());
    o.println("Avvii totali: " + String(sysBootCount()));
  }
  else if (c == "free") {
    o.println("RAM   libera " + String(ESP.getFreeHeap() / 1024) + " KB su " + String(ESP.getHeapSize() / 1024) + " KB");
    o.println("PSRAM libera " + String(ESP.getFreePsram() / 1024) + " KB su " + String(ESP.getPsramSize() / 1024) + " KB");
  }
  else if (c == "df") {
    o.println("/flash  usati " + String(LittleFS.usedBytes() / 1024) + " KB su " + String(LittleFS.totalBytes() / 1024) + " KB");
  }
  else if (c == "temp") o.println("Temperatura CPU: " + fmtTemp(sysCpuTemp()));
  else if (c == "top") {
    o.println("CPU " + String(sysCpuPercent()) + "%  Temp " + fmtTemp(sysCpuTemp()) + "  " + String(getCpuFrequencyMhz()) + " MHz");
    o.println("RAM libera " + String(ESP.getFreeHeap() / 1024) + " KB  PSRAM libera " + String(ESP.getFreePsram() / 1024) + " KB");
  }
  else if (c == "ps") o.print(sysTasksText());
  else if (c == "ls") {
    String j = fsListJson(a1.length() ? a1 : "/");
    if (j.indexOf("\"ok\":true") < 0) { o.println("Cartella non trovata"); }
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
    if (!a1.length()) { o.println("Uso: cat <file>"); return; }
    String t, err;
    if (!fsReadText(a1, t, err)) { o.println(err); return; }
    o.println(t);
  }
  else if (c == "mkdir") {
    String err; if (!a1.length()) { o.println("Uso: mkdir <cartella>"); return; }
    o.println(fsMkdir(a1, err) ? "Creata" : err);
  }
  else if (c == "rm") {
    String err; if (!a1.length()) { o.println("Uso: rm <file o cartella>"); return; }
    o.println(fsRemove(a1, err) ? "Eliminato" : err);
  }
  else if (c == "mv" || c == "cp") {
    String a2 = argAt(line, 2), err;
    if (!a1.length() || !a2.length()) { o.println("Uso: " + c + " <da> <a>"); return; }
    bool r = (c == "mv") ? fsRename(a1, a2, err) : fsCopy(a1, a2, err);
    o.println(r ? "Fatto" : err);
  }
  else if (c == "write") {
    String txt = restFrom(line, 2), err;
    if (!a1.length()) { o.println("Uso: write <file> <testo>"); return; }
    o.println(fsWriteText(a1, txt, err) ? "Scritto" : err);
  }
  else if (c == "date") o.println(timeNowStr() + "  (" + cfg.tzName + ")");
  else if (c == "ntp") {
    if (a1 == "sync") { timeApply(); o.println("Sincronizzazione NTP richiesta"); }
    else o.println(timeJson());
  }
  else if (c == "cpu") {
    if (!a1.length()) { o.println("CPU a " + String(getCpuFrequencyMhz()) + " MHz, modo " + (cfg.cpuMhz ? String(cfg.cpuMhz) + " fisso" : String("automatico"))); return; }
    int m = (a1 == "auto") ? 0 : a1.toInt();
    if (m != 0 && m != 80 && m != 160 && m != 240) { o.println("Uso: cpu auto|80|160|240"); return; }
    cfg.cpuMhz = m; cfgSave(); sysApplyCpuMode(); o.println("Modo CPU: " + a1);
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
    else { o.println("Uso: led state|heartbeat|fixed|off"); return; }
    ledApplyConfig(); cfgSave(); o.println("LED: " + a1);
  }
  else if (c == "led-color") {
    if (a1.length() != 6) { o.println("Uso: led-color RRGGBB"); return; }
    cfg.ledColor = (uint32_t)strtoul(a1.c_str(), NULL, 16) & 0xFFFFFF;
    cfgSave(); o.println("Colore: " + hex6(cfg.ledColor));
  }
  else if (c == "led-bright") {
    int v = a1.toInt();
    if (!a1.length() || v < 0 || v > 255) { o.println("Uso: led-bright 0-255"); return; }
    cfg.ledBrightness = v; cfgSave(); o.println("Luminosita: " + String(v));
  }
  else if (c == "led2") {
    if (a1 == "off") cfg.led2Mode = 0;
    else if (a1 == "on") cfg.led2Mode = 1;
    else if (a1 == "heartbeat") cfg.led2Mode = 2;
    else {
      const char* md[] = {"off", "on", "heartbeat"};
      o.println("Stato: " + String(md[cfg.led2Mode]) + ", pin " + String(cfg.led2Pin) + ", luminosita " + String(cfg.led2Bright) + ", livello " + String(cfg.led2Invert ? "BASSO (invertito)" : "alto (normale)"));
      o.println("Uso: led2 off|on|heartbeat   (led2-pin <n>, led2-bright <0-255>, led2-invert on|off)");
      return;
    }
    ledApplyConfig(); cfgSave(); o.println("LED aggiuntivo: " + a1 + " (pin " + String(cfg.led2Pin) + ")");
  }
  else if (c == "led2-invert") {
    if (a1 != "on" && a1 != "off") { o.println("Uso: led2-invert on|off   (on = si accende con livello basso)"); return; }
    cfg.led2Invert = (a1 == "on"); ledApplyConfig(); cfgSave();
    o.println(String("Livello LED aggiuntivo: ") + (cfg.led2Invert ? "BASSO (invertito)" : "alto (normale)"));
  }
  else if (c == "led2-pin") {
    int p = a1.toInt();
    if (!a1.length() || !led2PinAllowed(p)) { o.println("Pin non ammesso (usa 1-18, 21 oppure 38-47)"); return; }
    if (p != cfg.led2Pin && pinIsUsed(p)) { o.println("Pin gia usato da altro"); return; }
    cfg.led2Pin = p; ledApplyConfig(); cfgSave(); o.println("Pin LED aggiuntivo: " + String(p));
  }
  else if (c == "led2-bright") {
    int v = a1.toInt();
    if (!a1.length() || v < 0 || v > 255) { o.println("Uso: led2-bright 0-255"); return; }
    cfg.led2Bright = v; cfgSave(); o.println("Luminosita LED aggiuntivo: " + String(v));
  }
  else if (c == "hostname") {
    if (!a1.length()) { o.println(cfg.hostname); return; }
    a1.toLowerCase();
    if (!hostnameValid(a1)) { o.println("Nome non valido (1-32: lettere, numeri, trattino)"); return; }
    cfg.hostname = a1; cfgSave(); netReconfigure(); o.println("Nome host: " + a1);
  }
  else if (c == "domain") {
    if (!a1.length()) { o.println(cfg.domain.length() ? cfg.domain : String("(nessun dominio)")); return; }
    if (a1 == "-") a1 = "";
    a1.toLowerCase();
    if (!domainValid(a1)) { o.println("Dominio non valido"); return; }
    cfg.domain = a1; cfgSave(); o.println(a1.length() ? "Dominio: " + a1 : String("Dominio tolto"));
  }
  else if (c == "pins") {
    for (int i = 0; i < pinCount(); i++) {
      const PinInfo* p = pinAt(i);
      o.println("GPIO" + String(p->gpio) + "  " + p->owner + "  " + p->note);
    }
  }
  else if (c == "config") o.print(cfgExport(false));
  else if (c == "passwd") {
    String np = restFrom(line, 1);
    if (np.length() < 6) { o.println("Password troppo corta (min 6)"); return; }
    authSetPassword(np); cfgSave();
    serialAuthSet(true);
    o.println("Password cambiata");
  }
  else if (c == "log") o.print(logGet(30));
  else if (c == "factory-reset") {
    o.println("Azzero tutto e riavvio...");
    cfgFactoryReset(); delay(300); ESP.restart();
  }
  else if (c == "reboot") { o.println("Riavvio..."); delay(300); ESP.restart(); }
  else o.println("Comando sconosciuto. Scrivi 'help'.");
}

// ---------- shell seriale ----------
static String g_buf;
static bool g_waitPass = false;
static bool g_waitNew = false;

static void prompt() { Serial.print(serialAuthed() || !authIsSet() ? "vesevos> " : "password: "); }

void shellSerialPoll() {
  static bool first = true;
  if (first) { first = false; Serial.println(); Serial.println(String(VOS_NAME) + " " + VOS_VERSION + " - scrivi 'help'");
    if (!authIsSet()) Serial.println("Nessuna password: impostala dalla pagina web (primo accesso)."); prompt(); }
  while (Serial.available()) {
    char ch = Serial.read();
    if (ch == '\r' || ch == '\n') {
      String l = g_buf; g_buf = "";
      if (l.length() == 0 && ch == '\n') continue;
      Serial.println();
      if (authIsSet() && !serialAuthed()) {
        if (authLocked()) Serial.println("Bloccato per troppi errori, riprova tra un minuto.");
        else if (authCheck(l)) { serialAuthSet(true); Serial.println("Accesso eseguito."); }
        else Serial.println("Password errata.");
      } else if (l == "logout") { serialAuthSet(false); Serial.println("Uscito."); }
      else shellExec(l, Serial, true);
      prompt();
    } else if (ch == 8 || ch == 127) { if (g_buf.length()) g_buf.remove(g_buf.length() - 1); }
    else if (ch >= 32 && ch < 127 && g_buf.length() < 200) {
      g_buf += ch;
      if (serialAuthed() || !authIsSet()) Serial.print(ch);   // niente eco della password
    }
  }
}
