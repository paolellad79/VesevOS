// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_config.cpp
#include "vos_config.h"
#include "vos_i18n.h"
#include "vos_util.h"
#include "vos_log.h"
#include <LittleFS.h>

#define CFG_FILE "/vesevos.conf"
#define CFG_TMP  "/vesevos.conf.tmp"
#define CFG_BAK  "/vesevos.conf.bak"

VosConfig cfg;

void cfgDefaults() {
  cfg.hostname = "vesevos"; cfg.domain = ""; cfg.lang = "it";
  cfg.apSsid = "VesevOS";
  cfg.serialAuth = true;
  cfg.apPass = "vesevos123";
  cfg.staEnabled = false;
  cfg.staSsid = ""; cfg.staPass = "";
  cfg.staDhcp = true;
  cfg.ip = "192.168.1.50"; cfg.mask = "255.255.255.0";
  cfg.gw = "192.168.1.1"; cfg.dns1 = "192.168.1.1"; cfg.dns2 = "8.8.8.8";
  cfg.authSalt = ""; cfg.authHash = "";     // vuoto = primo accesso
  cfg.ledMode = LED_STATE;
  cfg.ledColor = 0x0000FF;
  cfg.ledBrightness = 40;
  cfg.ledPin = VOS_PIN_LED_RGB;
  cfg.banFails = 5; cfg.banSecs = 60;
  cfg.mqttAuto = false; cfg.mqttHost = ""; cfg.mqttUser = ""; cfg.mqttPass = ""; cfg.mqttPrefix = ""; cfg.mqttPort = 1883; cfg.mqttEvery = 30; cfg.mqttHa = true;
  cfg.airOn = 0; cfg.airExit = 0; cfg.airUntil = 0; cfg.airAt = 0;
  cfg.ntpOn = true; cfg.ntpServe = false; cfg.ntpEvery = 60;
  cfg.ntpServer = "pool.ntp.org";
  cfg.tz = "CET-1CEST,M3.5.0,M10.5.0/3"; cfg.tzName = "Europe/Rome";
  cfg.cpuMhz = 0;
  cfg.dateFmt = 0; cfg.timeFmt = 0; cfg.tempUnit = 0;
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

String cfgExport(bool withSecrets) {
  String s;
  s += "# VesevOS config\n";
  s += "config system 'system'\n";
  opt(s, "hostname", cfg.hostname);
  opt(s, "domain", cfg.domain);
  opt(s, "cpu", String(cfg.cpuMhz));
  opt(s, "lang", cfg.lang);
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
  opt(s, "salt", withSecrets ? cfg.authSalt : String(""));
  opt(s, "hash", withSecrets ? cfg.authHash : String(""));
  opt(s, "serial", cfg.serialAuth ? "1" : "0");
  opt(s, "banfails", String(cfg.banFails));
  opt(s, "bansecs", String((unsigned long)cfg.banSecs));
  s += "\nconfig led 'led'\n";
  opt(s, "mode", String(cfg.ledMode));
  opt(s, "color", String(cfg.ledColor));
  opt(s, "brightness", String(cfg.ledBrightness));
  opt(s, "pin", String(cfg.ledPin));
  s += "\nconfig mqtt 'mqtt'\n";
  opt(s, "auto", cfg.mqttAuto ? "1" : "0");
  opt(s, "host", cfg.mqttHost);
  opt(s, "port", String(cfg.mqttPort));
  opt(s, "user", cfg.mqttUser);
  opt(s, "pass", withSecrets ? cfg.mqttPass : String(""));
  opt(s, "prefix", cfg.mqttPrefix);
  opt(s, "every", String(cfg.mqttEvery));
  opt(s, "ha", cfg.mqttHa ? "1" : "0");
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
  s += "\nconfig sd 'sd'\n";
  opt(s, "enabled", cfg.sdEnabled ? "1" : "0");
  opt(s, "cs", String(cfg.sdCs)); opt(s, "sck", String(cfg.sdSck));
  opt(s, "miso", String(cfg.sdMiso)); opt(s, "mosi", String(cfg.sdMosi));
  return s;
}

// Applica una chiave. section = nome sezione, k = chiave
static void applyKey(const String& sec, const String& k, const String& v) {
  if (sec == "system") { if (k == "hostname" && hostnameValid(v)) cfg.hostname = v; else if (k == "domain" && domainValid(v)) cfg.domain = v; else if (k == "cpu") { int m = v.toInt(); if (m == 0 || m == 80 || m == 160 || m == 240) cfg.cpuMhz = m; } else if (k == "lang") { if (langCodeValid(v)) cfg.lang = v; } }
  else if (sec == "ap") {
    if (k == "ssid" && v.length()) cfg.apSsid = v;
    else if (k == "pass" && v.length() >= 8) cfg.apPass = v;
  } else if (sec == "sta") {
    if (k == "enabled") cfg.staEnabled = (v == "1");
    else if (k == "ssid") cfg.staSsid = v;
    else if (k == "pass") { if (v.length()) cfg.staPass = v; }
    else if (k == "dhcp") cfg.staDhcp = (v != "0");
    else if (k == "ip") cfg.ip = v; else if (k == "mask") cfg.mask = v;
    else if (k == "gw") cfg.gw = v; else if (k == "dns1") cfg.dns1 = v;
    else if (k == "dns2") cfg.dns2 = v;
  } else if (sec == "auth") {
    if (k == "salt" && v.length()) cfg.authSalt = v;
    else if (k == "hash" && v.length()) cfg.authHash = v;
    else if (k == "serial") cfg.serialAuth = (v != "0");
    else if (k == "banfails") cfg.banFails = constrain(v.toInt(), 3, 20);
    else if (k == "bansecs") cfg.banSecs = constrain(v.toInt(), 10, 3600);
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
    else if (k == "user") cfg.mqttUser = v;
    else if (k == "pass") { if (v.length()) cfg.mqttPass = v; }
    else if (k == "prefix") cfg.mqttPrefix = v;
    else if (k == "every") cfg.mqttEvery = constrain(v.toInt(), 5, 3600);
    else if (k == "ha") cfg.mqttHa = (v == "1");
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

bool cfgSave() {
  String s = cfgExport(true);
  File f = LittleFS.open(CFG_TMP, "w");
  if (!f) { vlog("CFG: errore apertura tmp"); return false; }
  size_t w = f.print(s);
  f.close();
  if (w != s.length()) { vlog("CFG: scrittura incompleta"); LittleFS.remove(CFG_TMP); return false; }
  if (LittleFS.exists(CFG_FILE)) {
    LittleFS.remove(CFG_BAK);
    LittleFS.rename(CFG_FILE, CFG_BAK);
  }
  if (!LittleFS.rename(CFG_TMP, CFG_FILE)) { vlog("CFG: rename fallito"); return false; }
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

bool cfgLoad() {
  cfgDefaults();
  if (loadFile(CFG_FILE)) return true;
  if (loadFile(CFG_BAK)) { vlog("CFG: ripristinato da .bak"); return true; }
  return false;
}

void cfgFactoryReset() {
  LittleFS.remove(CFG_FILE);
  LittleFS.remove(CFG_BAK);
  LittleFS.remove(CFG_TMP);
  cfgDefaults();
}
