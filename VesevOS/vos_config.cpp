// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_config.cpp
#include "vos_config.h"
#include "vos_i18n.h"
#include "vos_util.h"
#include "vos_log.h"
#include "vos_audit.h"
#include <LittleFS.h>
#include <Preferences.h>

#define CFG_FILE "/vesevos.conf"
#define CFG_TMP  "/vesevos.conf.tmp"
#define CFG_BAK  "/vesevos.conf.bak"

VosConfig cfg;

void cfgDefaults() {
  cfg.hostname = "vesevos"; cfg.domain = ""; cfg.lang = "it";
  cfg.setupDone = false;
  cfg.apSsid = "VesevOS";
  cfg.serialAuth = true;
  cfg.serBaud = 115200; cfg.serEol = 0; cfg.serTx = 10; cfg.serEcho = true; cfg.serIn = true; cfg.serLogOut = true; cfg.serBanner = true;
  cfg.apQr = true;
  for (int i = 0; i < 49; i++) cfg.pinNote[i] = "";
  cfg.apPass = "";                          // creata al primo avvio, unica per ogni scheda (vedi cfgLoad)
  cfg.staEnabled = false;
  cfg.staSsid = ""; cfg.staPass = "";
  cfg.staDhcp = true;
  cfg.ip = "192.168.1.50"; cfg.mask = "255.255.255.0";
  cfg.gw = "192.168.1.1"; cfg.dns1 = "192.168.1.1"; cfg.dns2 = "8.8.8.8";
  cfg.authSalt = ""; cfg.authHash = "";     // solo per i file vecchi
  for (int i = 0; i < VOS_MAX_USERS; i++) { cfg.users[i].name = ""; cfg.users[i].salt = ""; cfg.users[i].hash = ""; cfg.users[i].role = ROLE_GUEST; cfg.users[i].on = false; cfg.users[i].mfa = ""; cfg.users[i].mfaOn = false; cfg.users[i].mfaLast = 0; cfg.users[i].rec = ""; }
  cfg.ledMode = LED_STATE;
  cfg.ledColor = 0x0000FF;
  cfg.ledBrightness = 40;
  cfg.ledPin = VOS_PIN_LED_RGB;
  cfg.banFails = 5; cfg.banSecs = 60; cfg.powBits = 14; cfg.mfaNoTime = 0;
  cfg.mqttAuto = false; cfg.mqttHost = ""; cfg.mqttUser = ""; cfg.mqttPass = ""; cfg.mqttPrefix = ""; cfg.mqttPort = 1883; cfg.mqttEvery = 30; cfg.mqttHa = true; cfg.mqttTls = false;
  cfg.airOn = 0; cfg.airExit = 0; cfg.airUntil = 0; cfg.airAt = 0;
  cfg.ntpOn = true; cfg.ntpServe = false; cfg.ntpEvery = 60;
  cfg.ntpServer = "pool.ntp.org";
  cfg.tz = "CET-1CEST,M3.5.0,M10.5.0/3"; cfg.tzName = "Europe/Rome";
  cfg.cpuMhz = 0; cfg.logLevel = 2; cfg.statOn = false; cfg.pwMode = 0; cfg.pwAwake = 15; cfg.pwSleep = 10;
  cfg.dateFmt = 0; cfg.timeFmt = 0; cfg.tempUnit = 0; cfg.weekStart = 0; cfg.decSep = 0;
  cfg.country = ""; cfg.antExt = 0; cfg.antGain = 0; cfg.txDbm = 0;
  cfg.fwMode = 0; cfg.fwNtp = false; cfg.fwN = 0;
  cfg.meshAuto = false; cfg.meshRole = 0; cfg.meshKey = ""; cfg.meshCh = 1;
  cfg.https = true;
  cfg.apOn = true; cfg.apCaptive = true; cfg.httpOn = true; cfg.httpPort = 80; cfg.httpsPort = 443;
  cfg.dhcpOn = true; cfg.dhcpLease = 120; cfg.mdnsOn = true;
  cfg.wdTask = true; cfg.wdNet = true; cfg.wdRam = true; cfg.wdNetMin = 10; cfg.wdRamKb = 20; cfg.wdUpDays = 0; cfg.wdAt = -1; cfg.wdDays = 0x7F;
  cfg.sdEnabled = false;
  cfg.sdCs = 10; cfg.sdSck = 12; cfg.sdMiso = 13; cfg.sdMosi = 11;
}

static String q(const String& v) {            // 'valore' con escape OpenWrt
  String o = "'";
  for (size_t i = 0; i < v.length(); i++) {
    if (v[i] == '\'') o += "'\\''"; else o += v[i];
  }
  return o + "'";
}

static void opt(String& s, const char* k, const String& v) {
  s += "\toption "; s += k; s += " "; s += q(v); s += "\n";
}

// "1.2.3.4" <-> numero (ordine umano: primo numero nel byte alto)
static String fwRange(const FwRule& r) { return r.a == r.b ? ipToStr(r.a) : ipToStr(r.a) + "-" + ipToStr(r.b); }

String cfgExport(bool withSecrets) {
  String s;
  s += "# VesevOS config\n";
  s += "config system 'system'\n";
  opt(s, "hostname", cfg.hostname);
  opt(s, "domain", cfg.domain);
  opt(s, "cpu", String(cfg.cpuMhz));
  opt(s, "stat", cfg.statOn ? "1" : "0");
  opt(s, "loglv", String((int)cfg.logLevel));
  opt(s, "serbaud", String((unsigned long)cfg.serBaud));
  opt(s, "sereol", String((int)cfg.serEol));
  opt(s, "sertx", String((int)cfg.serTx));
  opt(s, "serecho", cfg.serEcho ? "1" : "0");
  opt(s, "serin", cfg.serIn ? "1" : "0");
  opt(s, "serlog", cfg.serLogOut ? "1" : "0");
  opt(s, "serban", cfg.serBanner ? "1" : "0");
  opt(s, "pwmode", String(cfg.pwMode));
  opt(s, "pwawake", String(cfg.pwAwake));
  opt(s, "pwsleep", String(cfg.pwSleep));
  opt(s, "lang", cfg.lang);
  opt(s, "setup", cfg.setupDone ? "1" : "0");
  opt(s, "https", cfg.https ? "1" : "0");
  s += "\nconfig svc 'svc'\n";
  opt(s, "apon", cfg.apOn ? "1" : "0");
  opt(s, "captive", cfg.apCaptive ? "1" : "0");
  opt(s, "dhcp", cfg.dhcpOn ? "1" : "0");
  opt(s, "lease", String(cfg.dhcpLease));
  opt(s, "mdns", cfg.mdnsOn ? "1" : "0");
  opt(s, "httpon", cfg.httpOn ? "1" : "0");
  opt(s, "httpport", String(cfg.httpPort));
  opt(s, "httpsport", String(cfg.httpsPort));
  s += "\nconfig region 'region'\n";
  opt(s, "country", cfg.country);
  opt(s, "antenna", String(cfg.antExt));
  opt(s, "gain", String(cfg.antGain));
  opt(s, "txpower", String(cfg.txDbm));
  opt(s, "weekstart", String(cfg.weekStart));
  opt(s, "decsep", String(cfg.decSep));
  s += "\nconfig ap 'ap'\n";
  opt(s, "ssid", cfg.apSsid);
  opt(s, "pass", withSecrets ? cfg.apPass : String(""));
  s += "\nconfig client 'sta'\n";
  opt(s, "enabled", cfg.staEnabled ? "1" : "0");
  opt(s, "ssid", cfg.staSsid);
  opt(s, "pass", withSecrets ? cfg.staPass : String(""));
  opt(s, "dhcp", cfg.staDhcp ? "1" : "0");
  opt(s, "ip", cfg.ip); opt(s, "mask", cfg.mask); opt(s, "gw", cfg.gw);
  opt(s, "dns1", cfg.dns1); opt(s, "dns2", cfg.dns2);
  s += "\nconfig auth 'auth'\n";
  opt(s, "serial", cfg.serialAuth ? "1" : "0");
  opt(s, "apqr", cfg.apQr ? "1" : "0");
  opt(s, "banfails", String(cfg.banFails));
  opt(s, "bansecs", String((unsigned long)cfg.banSecs));
  opt(s, "powbits", String(cfg.powBits));
  opt(s, "mfanotime", String(cfg.mfaNoTime));
  for (int i = 0; i < VOS_MAX_USERS; i++) {
    const VosUser& u = cfg.users[i];
    if (!u.name.length()) continue;
    s += "\nconfig user 'user" + String(i) + "'\n";
    opt(s, "name", u.name);
    opt(s, "role", String(u.role));
    opt(s, "on", u.on ? "1" : "0");
    opt(s, "salt", withSecrets ? u.salt : String(""));
    opt(s, "hash", withSecrets ? u.hash : String(""));
    opt(s, "mfaon", u.mfaOn ? "1" : "0");
    opt(s, "mfa", withSecrets ? u.mfa : String(""));
    opt(s, "mfalast", String((unsigned long)u.mfaLast));
    opt(s, "rec", withSecrets ? u.rec : String(""));
  }
  s += "\nconfig firewall 'firewall'\n";
  opt(s, "mode", String(cfg.fwMode));
  opt(s, "ntp", cfg.fwNtp ? "1" : "0");
  for (int i = 0; i < cfg.fwN && i < VOS_FW_MAX; i++)
    opt(s, ("rule" + String(i)).c_str(), fwRange(cfg.fw[i]) + "|" + (cfg.fw[i].on ? "1" : "0") + "|" + cfg.fw[i].name);
  s += "\nconfig led 'led'\n";
  opt(s, "mode", String(cfg.ledMode));
  opt(s, "color", String(cfg.ledColor));
  opt(s, "brightness", String(cfg.ledBrightness));
  opt(s, "pin", String(cfg.ledPin));
  s += "\nconfig mqtt 'mqtt'\n";
  opt(s, "auto", cfg.mqttAuto ? "1" : "0");
  opt(s, "host", cfg.mqttHost);
  opt(s, "port", String(cfg.mqttPort));
  opt(s, "tls", cfg.mqttTls ? "1" : "0");
  opt(s, "user", cfg.mqttUser);
  opt(s, "pass", withSecrets ? cfg.mqttPass : String(""));
  opt(s, "prefix", cfg.mqttPrefix);
  opt(s, "every", String(cfg.mqttEvery));
  opt(s, "ha", cfg.mqttHa ? "1" : "0");
  s += "\nconfig mesh 'mesh'\n";
  opt(s, "auto", cfg.meshAuto ? "1" : "0");
  opt(s, "role", String(cfg.meshRole));
  opt(s, "channel", String(cfg.meshCh));
  opt(s, "key", withSecrets ? cfg.meshKey : String(""));
  s += "\nconfig airplane 'airplane'\n";
  opt(s, "on", String(cfg.airOn));
  opt(s, "exit", String(cfg.airExit));
  opt(s, "until", String((unsigned long)cfg.airUntil));
  opt(s, "at", String(cfg.airAt));
  s += "\nconfig time 'time'\n";
  opt(s, "ntp", cfg.ntpOn ? "1" : "0");
  opt(s, "server", cfg.ntpServer);
  opt(s, "serve", cfg.ntpServe ? "1" : "0");
  opt(s, "every", String((unsigned long)cfg.ntpEvery));
  opt(s, "tz", cfg.tz);
  opt(s, "tzname", cfg.tzName);
  opt(s, "datefmt", String(cfg.dateFmt));
  opt(s, "timefmt", String(cfg.timeFmt));
  opt(s, "tempunit", String(cfg.tempUnit));
  s += "\nconfig watchdog 'watchdog'\n";
  opt(s, "task", cfg.wdTask ? "1" : "0");
  opt(s, "net", cfg.wdNet ? "1" : "0");
  opt(s, "ram", cfg.wdRam ? "1" : "0");
  opt(s, "netmin", String(cfg.wdNetMin));
  opt(s, "ramkb", String(cfg.wdRamKb));
  opt(s, "updays", String(cfg.wdUpDays));
  opt(s, "at", String(cfg.wdAt));
  opt(s, "days", String(cfg.wdDays));
  s += "\nconfig pins 'pins'\n";
  for (int i = 0; i < 49; i++) if (cfg.pinNote[i].length()) opt(s, ("n" + String(i)).c_str(), cfg.pinNote[i]);
  s += "\nconfig sd 'sd'\n";
  opt(s, "enabled", cfg.sdEnabled ? "1" : "0");
  opt(s, "cs", String(cfg.sdCs)); opt(s, "sck", String(cfg.sdSck));
  opt(s, "miso", String(cfg.sdMiso)); opt(s, "mosi", String(cfg.sdMosi));
  return s;
}

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
static bool g_hadSetupKey = false;      // il file ha la riga "setup" (scritta dalla 1.7.1 in poi)
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
    else if (k == "setup") { cfg.setupDone = (v == "1"); g_hadSetupKey = true; }
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
    else if (k == "serve") cfg.ntpServe = (v == "1");
    else if (k == "every") { long m = v.toInt(); if (m >= 0 && m <= 10080) cfg.ntpEvery = m; }
    else if (k == "server" && v.length()) cfg.ntpServer = v;
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

// ---- chi cambia (per il registro delle modifiche) ----
static String g_origin = "sistema";
void cfgSetOrigin(const String& who) { g_origin = who; }
String cfgOrigin() { return g_origin; }

// ---- impronta del file salvato (per accorgersi di modifiche fatte fuori dal pannello) ----
static String fileHash(const char* path) {
  File f = LittleFS.open(path, "r");
  if (!f) return "";
  String t = f.readString(); f.close();
  return sha256Hex(t);
}
static void hashSave(const String& h) { Preferences p; if (p.begin("vos", false)) { p.putString("cfgh", h); p.end(); } }
bool cfgFileChanged() {
  if (!LittleFS.exists(CFG_FILE)) return false;
  Preferences p; String saved;
  if (p.begin("vos", true)) { saved = p.getString("cfgh", ""); p.end(); }
  if (!saved.length()) return false;                 // prima volta: niente da confrontare
  return fileHash(CFG_FILE) != saved;
}

static String g_lastPub;     // ultimo testo salvato SENZA password (per il registro delle modifiche)

static bool g_saveOk = true;
bool cfgLastSaveOk() { return g_saveOk; }
static bool saveFail(const char* why) { vlog("CFG: %s", why); g_saveOk = false; auditRefresh(); return false; }

bool cfgSave() {
  auditFix();                                        // valori fuori regola: corretti prima di salvare
  String s = cfgExport(true);
  File f = LittleFS.open(CFG_TMP, "w");
  if (!f) return saveFail("errore apertura tmp");
  size_t w = f.print(s);
  f.close();
  if (w != s.length()) { LittleFS.remove(CFG_TMP); return saveFail("scrittura incompleta (memoria piena?)"); }
  if (LittleFS.exists(CFG_FILE)) {
    LittleFS.remove(CFG_BAK);
    LittleFS.rename(CFG_FILE, CFG_BAK);
  }
  if (!LittleFS.rename(CFG_TMP, CFG_FILE)) return saveFail("rename fallito");
  g_saveOk = true;
  hashSave(sha256Hex(s));
  String pub = cfgExport(false);
  if (g_lastPub.length()) auditDiff(g_lastPub, pub, g_origin);
  g_lastPub = pub;
  auditRefresh();
  return true;
}

static bool loadFile(const char* path) {
  File f = LittleFS.open(path, "r");
  if (!f) return false;
  String t = f.readString();
  f.close();
  String err;
  if (!cfgImport(t, err)) { vlog("CFG: %s: %s", path, err.c_str()); return false; }
  return true;
}

// Password casuale per l'hotspot: 12 caratteri senza simboli che si confondono, a gruppi di 4 ("k7Hm-pQ4x-Tr9a")
String cfgNewApPass() {
  static const char* A = "abcdefghjkmnpqrstuvwxyz23456789";   // solo minuscole e cifre, senza lettere simili (facile da scrivere sul telefono)
  size_t n = strlen(A);
  String p;
  for (int i = 0; i < 12; i++) {
    uint32_t r;
    do { r = esp_random() & 0xFF; } while (r >= (256 / n) * n);   // nessuna preferenza per alcune lettere
    p += A[r % n];
  }
  return p;
}

bool cfgApPassWeak(const String& p) {
  return p.length() < 8 || p == "vesevos123" || p == "12345678" || p == "password" || p == "vesevos1";
}

bool cfgLoad() {
  cfgDefaults(); g_hadSetupKey = false;
  bool found = loadFile(CFG_FILE);
  if (!found && loadFile(CFG_BAK)) { vlog("CFG: ripristinato da .bak"); found = true; }
  // file della 1.7.0: la password diventa l'utente "admin" e la scheda risulta gia configurata
  bool any = false;
  for (int i = 0; i < VOS_MAX_USERS; i++) if (cfg.users[i].name.length()) any = true;
  if (!any && cfg.authHash.length() == 64 && cfg.authSalt.length()) {
    cfg.users[0].name = "admin"; cfg.users[0].salt = cfg.authSalt; cfg.users[0].hash = cfg.authHash;
    cfg.users[0].role = ROLE_ADMIN; cfg.users[0].on = true;
    cfg.setupDone = true;
    vlog("CFG: password della versione precedente -> utente 'admin'");
  }
  cfg.authSalt = ""; cfg.authHash = "";
  // file di prima della 1.7.1 (senza la riga "setup") con almeno un utente e il Wi-Fi di casa: gia in uso, la guida non serve.
  // Dalla 1.7.2 il Wi-Fi salvato da solo NON basta piu: la guida deve essere finita davvero.
  if (found && !g_hadSetupKey && !cfg.setupDone && cfg.staEnabled && cfg.staSsid.length()) {
    for (int i = 0; i < VOS_MAX_USERS; i++) if (cfg.users[i].name.length() && cfg.users[i].hash.length()) { cfg.setupDone = true; break; }
  }
  // sicurezza: ne HTTP ne HTTPS acceso, o stessa porta -> si torna ai valori di fabbrica
  if ((!cfg.httpOn && !cfg.https) || cfg.httpPort == cfg.httpsPort) { cfgSvcRecover(); vlog("CFG: servizi web non validi, valori di fabbrica"); }
  // guida non finita (primo avvio o reset): hotspot, HTTP, HTTPS e porte standard, cosi si entra sempre
  if (!cfg.setupDone) cfgSvcRecover();
  // hotspot: mai una password uguale per tutti (leggi UE RED/EN 18031, CRA; Regno Unito PSTI)
  if (cfgApPassWeak(cfg.apPass)) {
    cfg.apPass = cfgNewApPass();
    vlog("CFG: nuova password unica per l'hotspot (vedi la schermata di benvenuto sulla seriale)");
    cfgSetOrigin("primo avvio");
    cfgSave();
  }
  g_lastPub = cfgExport(false);
  return found;
}

bool cfgSvcRecover() {
  bool ch = !cfg.apOn || !cfg.apCaptive || !cfg.dhcpOn || !cfg.httpOn || !cfg.https || cfg.httpPort != 80 || cfg.httpsPort != 443;
  cfg.apOn = true; cfg.apCaptive = true; cfg.dhcpOn = true; cfg.httpOn = true; cfg.https = true; cfg.httpPort = 80; cfg.httpsPort = 443;
  return ch;
}

String cfgSvcCheck(bool apOn, bool httpOn, int httpPort, bool httpsOn, int httpsPort) {
  if (!httpOn && !httpsOn) return tr("Deve restare acceso almeno un protocollo web (HTTP o HTTPS), altrimenti resti chiuso fuori");
  if (httpOn && httpPort != 80 && (httpPort < 1024 || httpPort > 65535)) return tr("Porta HTTP non valida: 80 oppure da 1024 a 65535");
  if (httpsOn && httpsPort != 443 && (httpsPort < 1024 || httpsPort > 65535)) return tr("Porta HTTPS non valida: 443 oppure da 1024 a 65535");
  if (httpOn && httpsOn && httpPort == httpsPort) return tr("HTTP e HTTPS non possono usare la stessa porta");
  if (!apOn && !(cfg.staEnabled && cfg.staSsid.length())) return tr("Per spegnere il Punto di accesso serve prima la Wi-Fi di casa configurata");
  return "";
}

void cfgFactoryReset() {
  LittleFS.remove(CFG_FILE);
  LittleFS.remove(CFG_BAK);
  LittleFS.remove(CFG_TMP);
  LittleFS.remove("/mqtt-ca.pem");
  Preferences p;
  if (p.begin("vos", false)) { p.remove("cfgh"); p.remove("audit"); p.end(); }
  if (p.begin("tls", false)) { p.clear(); p.end(); }
  cfgDefaults();
}
