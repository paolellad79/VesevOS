// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_config_import.cpp
#include "vos_config.h"
#include "vos_i18n.h"
#include "vos_util.h"
#include "vos_log.h"
#include "vos_audit.h"
#include "../drivers/vos_drv_fs.h"
#include <Preferences.h>
#include "vos_config_int.h"

static bool hexOnly(const String& v, size_t len) {
  if (v.length() != len) return false;
  for (size_t i = 0; i < v.length(); i++) { char c = v[i]; if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return false; }
  return true;
}

// "a.b.c.d" oppure "a.b.c.d-e.f.g.h" oppure "a.b.c.d/nn"
bool cfgParseRange(const String& txt, uint32_t& a, uint32_t& b) {
  String t = txt; t.trim();
  int d = t.indexOf('-'), sl = t.indexOf('/');
  if (d > 0) { if (!ipParse(t.substring(0, d), a) || !ipParse(t.substring(d + 1), b) || b < a) return false; return true; }
  if (sl > 0) {
    long n = t.substring(sl + 1).toInt();
    if (n < 8 || n > 32 || !ipParse(t.substring(0, sl), a)) return false;
    uint32_t m = n == 32 ? 0xFFFFFFFFUL : ~(0xFFFFFFFFUL >> n);
    a &= m; b = a | ~m; return true;
  }
  if (!ipParse(t, a)) return false;
  b = a; return true;
}

static int g_userSec = -1;   // sezione utente in lettura

// Applica una chiave. section = nome sezione, k = chiave
bool g_cfgHadSetupKey = false;      // il file ha la riga "setup" (scritta dalla 1.7.1 in poi)
static void applyKey(const String& sec, const String& k, const String& v) {
  if (sec == "system") {
    if (k == "hostname" && hostnameValid(v)) cfg.hostname = v;
    else if (k == "domain" && domainValid(v)) cfg.domain = v;
    else if (k == "stat") cfg.statOn = (v == "1");
    else if (k == "serbaud") { uint32_t b = (uint32_t)v.toInt(); if (b >= 1200 && b <= 2000000) cfg.serBaud = b; }
    else if (k == "sereol") cfg.serEol = (uint8_t)constrain((int)v.toInt(), 0, 2);
    else if (k == "sertx") cfg.serTx = (uint8_t)constrain((int)v.toInt(), 0, 200);
    else if (k == "serecho") cfg.serEcho = (v != "0");
    else if (k == "serin") cfg.serIn = (v != "0");
    else if (k == "serlog") cfg.serLogOut = (v != "0");
    else if (k == "serban") cfg.serBanner = (v != "0");
    else if (k == "loglv") cfg.logLevel = (uint8_t)constrain((int)v.toInt(), 0, 3);
    else if (k == "pwmode") cfg.pwMode = constrain(v.toInt(), 0, 2);
    else if (k == "pwawake") cfg.pwAwake = constrain(v.toInt(), 10, 1440);
    else if (k == "pwsleep") cfg.pwSleep = constrain(v.toInt(), 1, 10080);
    else if (k == "cpu") { int m = v.toInt(); if (m == 0 || m == 80 || m == 160 || m == 240) cfg.cpuMhz = m; }
    else if (k == "lang") { if (langCodeValid(v)) cfg.lang = v; }
    else if (k == "setup") { cfg.setupDone = (v == "1"); g_cfgHadSetupKey = true; }
    else if (k == "https") cfg.https = (v != "0");
  }
  else if (sec == "pins") {
    if (k.length() >= 2 && k[0] == 'n') { int g = k.substring(1).toInt(); if (g >= 0 && g < 49 && v.length() <= 24) cfg.pinNote[g] = v; }
  }
  else if (sec == "svc") {
    if (k == "apon") cfg.apOn = (v != "0");
    else if (k == "captive") cfg.apCaptive = (v != "0");
    else if (k == "dhcp") cfg.dhcpOn = (v != "0");
    else if (k == "lease") { int m = v.toInt(); if (m >= 10 && m <= 1440) cfg.dhcpLease = m; }
    else if (k == "mdns") cfg.mdnsOn = (v != "0");
    else if (k == "httpon") cfg.httpOn = (v != "0");
    else if (k == "httpport") { int n = v.toInt(); if (n == 80 || (n >= 1024 && n <= 65535)) cfg.httpPort = n; }
    else if (k == "httpsport") { int n = v.toInt(); if (n == 443 || (n >= 1024 && n <= 65535)) cfg.httpsPort = n; }
  }
  else if (sec == "region") {
    if (k == "country") { if (v.length() == 0 || (v.length() == 2 && isupper(v[0]) && isupper(v[1]))) cfg.country = v; }
    else if (k == "antenna") cfg.antExt = v == "1" ? 1 : 0;
    else if (k == "gain") cfg.antGain = constrain(v.toInt(), 0, 15);
    else if (k == "txpower") cfg.txDbm = constrain(v.toInt(), 0, 20);
    else if (k == "weekstart") cfg.weekStart = v == "1" ? 1 : 0;
    else if (k == "decsep") cfg.decSep = v == "1" ? 1 : 0;
  }
  else if (sec == "ap") {
    if (k == "ssid" && v.length() && v.length() <= 32) cfg.apSsid = v;
    else if (k == "pass" && v.length() >= 8 && v.length() <= 63) cfg.apPass = v;
  } else if (sec == "sta") {
    if (k == "enabled") cfg.staEnabled = (v == "1");
    else if (k == "ssid") cfg.staSsid = v;
    else if (k == "pass") { if (v.length()) cfg.staPass = v; }
    else if (k == "dhcp") cfg.staDhcp = (v != "0");
    else if (k == "ip") cfg.ip = v; else if (k == "mask") cfg.mask = v;
    else if (k == "gw") cfg.gw = v; else if (k == "dns1") cfg.dns1 = v;
    else if (k == "dns2") cfg.dns2 = v;
  } else if (sec == "auth") {
    if (k == "salt" && v.length()) cfg.authSalt = v;          // file vecchi (1.7.0)
    else if (k == "hash" && v.length()) cfg.authHash = v;
    else if (k == "serial") cfg.serialAuth = (v != "0");
    else if (k == "apqr") cfg.apQr = (v != "0");
    else if (k == "banfails") cfg.banFails = constrain(v.toInt(), 3, 20);
    else if (k == "bansecs") cfg.banSecs = constrain(v.toInt(), 10, 3600);
    else if (k == "powbits") cfg.powBits = (v.toInt() == 0 || (v.toInt() >= 8 && v.toInt() <= 20)) ? v.toInt() : 14;
    else if (k == "mfanotime") cfg.mfaNoTime = constrain(v.toInt(), 0, 2);
  } else if (sec.startsWith("user")) {
    int i = sec.substring(4).toInt();
    if (i < 0 || i >= VOS_MAX_USERS || sec.length() > 5) return;
    VosUser& u = cfg.users[i];
    if (k == "name") { if (v.length() >= 3 && v.length() <= 20) u.name = v; }
    else if (k == "role") u.role = constrain(v.toInt(), 0, 2);
    else if (k == "on") u.on = (v == "1");
    else if (k == "salt" && v.length()) u.salt = v;
    else if (k == "hash" && hexOnly(v, 64)) u.hash = v;
    else if (k == "mfaon") u.mfaOn = (v == "1");
    else if (k == "mfa" && (v.length() == 0 || hexOnly(v, 40))) u.mfa = v;
    else if (k == "mfalast") u.mfaLast = (uint32_t)strtoul(v.c_str(), nullptr, 10);
    else if (k == "rec" && v.length() <= 8 * 17) u.rec = v;
  } else if (sec == "firewall") {
    if (k == "mode") cfg.fwMode = constrain(v.toInt(), 0, 3);
    else if (k == "ntp") cfg.fwNtp = (v == "1");
    else if (k.startsWith("rule") && cfg.fwN < VOS_FW_MAX) {
      int p1 = v.indexOf('|'), p2 = p1 >= 0 ? v.indexOf('|', p1 + 1) : -1;
      FwRule r;
      if (p1 < 0 || !cfgParseRange(v.substring(0, p1), r.a, r.b)) return;
      r.on = p2 > p1 ? v.substring(p1 + 1, p2) == "1" : true;
      r.name = p2 > p1 ? v.substring(p2 + 1) : String("");
      if (r.name.length() > 24) r.name = r.name.substring(0, 24);
      cfg.fw[cfg.fwN++] = r;
    }
  } else if (sec == "led") {
    if (k == "mode") cfg.ledMode = constrain(v.toInt(), 0, 3);
    else if (k == "color") cfg.ledColor = (uint32_t)strtoul(v.c_str(), NULL, 10) & 0xFFFFFF;
    else if (k == "brightness") cfg.ledBrightness = constrain(v.toInt(), 0, 255);
    else if (k == "pin") { int p = v.toInt(); if (p >= 0 && p <= 48) cfg.ledPin = p; }
  } else if (sec == "led2") {
    // LED aggiuntivo tolto nella 1.7.0: la sezione dei file vecchi si ignora
  } else if (sec == "mqtt") {
    if (k == "auto") cfg.mqttAuto = (v == "1");
    else if (k == "host") cfg.mqttHost = v;
    else if (k == "port") { long p = v.toInt(); if (p > 0 && p < 65536) cfg.mqttPort = p; }
    else if (k == "tls") cfg.mqttTls = (v == "1");
    else if (k == "user") cfg.mqttUser = v;
    else if (k == "pass") { if (v.length()) cfg.mqttPass = v; }
    else if (k == "prefix") cfg.mqttPrefix = v;
    else if (k == "every") cfg.mqttEvery = constrain(v.toInt(), 5, 3600);
    else if (k == "ha") cfg.mqttHa = (v == "1");
  } else if (sec == "mesh") {
    if (k == "auto") cfg.meshAuto = (v == "1");
    else if (k == "role") cfg.meshRole = constrain(v.toInt(), 0, 2);
    else if (k == "channel") cfg.meshCh = constrain(v.toInt(), 1, 13);
    else if (k == "key" && hexOnly(v, 64)) cfg.meshKey = v;
  } else if (sec == "airplane") {
    if (k == "on") cfg.airOn = (v == "1");
    else if (k == "exit") cfg.airExit = constrain(v.toInt(), 0, 3);
    else if (k == "until") cfg.airUntil = strtoul(v.c_str(), NULL, 10);
    else if (k == "at") cfg.airAt = constrain(v.toInt(), 0, 1439);
  } else if (sec == "time") {
    if (k == "ntp") cfg.ntpOn = (v == "1");
    else if (k == "ntpauto") cfg.ntpAutoOff = (v == "1");
    else if (k == "serve") cfg.ntpServe = (v == "1");
    else if (k == "every") { long m = v.toInt(); if (m >= 0 && m <= 10080) cfg.ntpEvery = m; }
    else if (k == "server" && v.length()) cfg.ntpServer = v;
    else if (k == "server2") cfg.ntpServer2 = v;
    else if (k == "tz" && v.length()) cfg.tz = v;
    else if (k == "tzname" && v.length()) cfg.tzName = v;
    else if (k == "datefmt") cfg.dateFmt = constrain(v.toInt(), 0, 2);
    else if (k == "timefmt") cfg.timeFmt = constrain(v.toInt(), 0, 1);
    else if (k == "tempunit") cfg.tempUnit = constrain(v.toInt(), 0, 1);
  } else if (sec == "watchdog") {
    if (k == "task") cfg.wdTask = (v != "0");
    else if (k == "net") cfg.wdNet = (v != "0");
    else if (k == "ram") cfg.wdRam = (v != "0");
    else if (k == "netmin") cfg.wdNetMin = constrain(v.toInt(), 2, 1440);
    else if (k == "ramkb") cfg.wdRamKb = constrain(v.toInt(), 8, 200);
    else if (k == "updays") cfg.wdUpDays = constrain(v.toInt(), 0, 365);
    else if (k == "at") cfg.wdAt = constrain(v.toInt(), -1, 1439);
    else if (k == "days") cfg.wdDays = v.toInt() & 0x7F;
  } else if (sec == "sd") {
    if (k == "enabled") cfg.sdEnabled = (v == "1");
    else if (k == "cs") cfg.sdCs = v.toInt(); else if (k == "sck") cfg.sdSck = v.toInt();
    else if (k == "miso") cfg.sdMiso = v.toInt(); else if (k == "mosi") cfg.sdMosi = v.toInt();
  }
}

// Legge un valore tra apici con escape '\''
static bool parseQuoted(const String& line, int pos, String& out) {
  out = "";
  while (pos < (int)line.length() && (line[pos] == ' ' || line[pos] == '\t')) pos++;
  if (pos >= (int)line.length()) return false;
  if (line[pos] != '\'') {                       // senza apici: fino a fine riga
    out = line.substring(pos); out.trim(); return true;
  }
  pos++;
  while (pos < (int)line.length()) {
    char c = line[pos];
    if (c == '\'') {
      if (line.startsWith("\\''", pos + 1)) { out += '\''; pos += 4; continue; }
      return true;
    }
    out += c; pos++;
  }
  return false;
}

bool cfgImport(const String& text, String& err) {
  String sec;
  int ln = 0, applied = 0;
  int i = 0;
  bool fwSeen = false;
  while (i <= (int)text.length()) {
    int e = text.indexOf('\n', i);
    if (e < 0) e = text.length();
    String line = text.substring(i, e);
    i = e + 1; ln++;
    line.replace("\r", "");
    String t = line; t.trim();
    if (t.length() == 0 || t[0] == '#') continue;
    if (t.startsWith("config ")) {
      String rest = t.substring(7); rest.trim();
      int sp = rest.indexOf(' ');
      String name;
      if (sp >= 0) { String tmp; parseQuoted(rest, sp + 1, tmp); name = tmp; }
      sec = name.length() ? name : rest;
      if (sec == "firewall" && !fwSeen) { fwSeen = true; cfg.fwN = 0; }   // la lista delle regole si sostituisce
      continue;
    }
    if (t.startsWith("option ")) {
      String rest = t.substring(7); rest.trim();
      int sp = rest.indexOf(' ');
      if (sp < 0) { err = trf("riga %d: option senza valore", ln); return false; }
      String k = rest.substring(0, sp), v;
      if (!parseQuoted(rest, sp + 1, v)) { err = trf("riga %d: apici non chiusi", ln); return false; }
      applyKey(sec, k, v); applied++;
      continue;
    }
    err = trf("riga %d: non capisco", ln);
    return false;
  }
  if (applied == 0) { err = tr("nessuna opzione trovata"); return false; }
  return true;
}
