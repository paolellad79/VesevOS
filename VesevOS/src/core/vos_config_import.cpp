// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_config_import.cpp
#include "vos_config.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include "vos_parse.h"
#include "vos_i18n.h"
#include "vos_util.h"
#include "vos_log.h"
#include "vos_audit.h"
#include "vos_config_int.h"
#include "vos_cpugov.h"

using std::string_view;

#define CFG_VAL_MAX 192   // un valore piu lungo non e valido per nessuna chiave: l'opzione si ignora

static long clampL(long v, long lo, long hi) { return v < lo ? lo : (v > hi ? hi : v); }

static bool hexOnly(string_view v, size_t len) {
  if (v.size() != len) return false;
  for (char c : v) { if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return false; }
  return true;
}

// "a.b.c.d" oppure "a.b.c.d-e.f.g.h" oppure "a.b.c.d/nn"
bool cfgParseRange(string_view txt, uint32_t& a, uint32_t& b) {
  string_view t = trimView(txt);
  size_t d = t.find('-'), sl = t.find('/');
  if (d != string_view::npos && d > 0) {
    if (!ipParse(t.substr(0, d), a) || !ipParse(t.substr(d + 1), b) || b < a) return false;
    return true;
  }
  if (sl != string_view::npos && sl > 0) {
    long n = viewToLong(t.substr(sl + 1));
    if (n < 8 || n > 32 || !ipParse(t.substr(0, sl), a)) return false;
    uint32_t m = n == 32 ? 0xFFFFFFFFUL : ~(0xFFFFFFFFUL >> n);
    a &= m; b = a | ~m; return true;
  }
  if (!ipParse(t, a)) return false;
  b = a; return true;
}


// Applica una chiave. sec = nome sezione, k = chiave, v = valore (con lo zero finale subito dopo la fine della vista)
bool g_cfgHadSetupKey = false;      // il file ha la riga "setup" (scritta dalla 1.7.1 in poi)
static void applyKey(string_view sec, string_view k, string_view v) {
  if (sec == "system") {
    if (k == "hostname" && hostnameValid(v)) cfg.hostname = v;
    else if (k == "domain" && domainValid(v)) cfg.domain = v;
    else if (k == "stat") cfg.statOn = (v == "1");
    else if (k == "btn1" || k == "btn2" || k == "btn3") { int a = viewToLong(v); if (a >= 0 && a <= 3) cfg.btnAct[k[3] - '1'] = (uint8_t)a; }
    else if (k == "serbaud") { uint32_t b = (uint32_t)viewToLong(v); if (b >= 1200 && b <= 2000000) cfg.serBaud = b; }
    else if (k == "sereol") cfg.serEol = (uint8_t)clampL(viewToLong(v), 0, 2);
    else if (k == "sertx") cfg.serTx = (uint8_t)clampL(viewToLong(v), 0, 200);
    else if (k == "serecho") cfg.serEcho = (v != "0");
    else if (k == "serin") cfg.serIn = (v != "0");
    else if (k == "serlog") cfg.serLogOut = (v != "0");
    else if (k == "serban") cfg.serBanner = (v != "0");
    else if (k == "loglv") cfg.logLevel = (uint8_t)clampL(viewToLong(v), 0, 3);
    else if (k == "pwmode") cfg.pwMode = clampL(viewToLong(v), 0, 2);
    else if (k == "pwawake") cfg.pwAwake = clampL(viewToLong(v), 10, 1440);
    else if (k == "pwsleep") cfg.pwSleep = clampL(viewToLong(v), 1, 10080);
    else if (k == "cpu") { int m = viewToLong(v); if (m == 0 || m == 80 || m == 160 || m == 240) cfg.cpuMhz = m; }
    else if (k == "cpugov") { int g = viewToLong(v); if (g >= 0 && g <= 2) cfg.cpuGov = (uint8_t)g; }
    else if (k == "cpuup") { int u = viewToLong(v); if (cpuGovParamsValid(u, cfg.cpuDown, cfg.cpuHold)) cfg.cpuUp = (uint8_t)u; }
    else if (k == "cpudown") { int d = viewToLong(v); if (cpuGovParamsValid(cfg.cpuUp, d, cfg.cpuHold)) cfg.cpuDown = (uint8_t)d; }
    else if (k == "cpuhold") { int h = viewToLong(v); if (cpuGovParamsValid(cfg.cpuUp, cfg.cpuDown, h)) cfg.cpuHold = (uint16_t)h; }
    else if (k == "lang") { if (langCodeValid(v)) cfg.lang = v; }
    else if (k == "setup") { cfg.setupDone = (v == "1"); g_cfgHadSetupKey = true; }
    else if (k == "https") cfg.https = (v != "0");
  }
  else if (sec == "pins") {
    if (k.size() >= 2 && k[0] == 'n') { int g = viewToLong(k.substr(1)); if (g >= 0 && g < VOS_GPIO_COUNT && v.size() <= 24) cfg.pinNote[g] = v; }
  }
  else if (sec == "svc") {
    if (k == "apon") cfg.apOn = (v != "0");
    else if (k == "captive") cfg.apCaptive = (v != "0");
    else if (k == "dhcp") cfg.dhcpOn = (v != "0");
    else if (k == "lease") { int m = viewToLong(v); if (m >= 10 && m <= 1440) cfg.dhcpLease = m; }
    else if (k == "mdns") cfg.mdnsOn = (v != "0");
    else if (k == "wps") cfg.wpsOn = (v == "1");
    else if (k == "wpspin") cfg.wpsPin = (v == "1");
    else if (k == "httpon") cfg.httpOn = (v != "0");
    else if (k == "redirect") cfg.httpRedirect = (v == "1");
    else if (k == "httpport") { int n = viewToLong(v); if (n == 80 || (n >= 1024 && n <= 65535)) cfg.httpPort = n; }
    else if (k == "httpsport") { int n = viewToLong(v); if (n == 443 || (n >= 1024 && n <= 65535)) cfg.httpsPort = n; }
  }
  else if (sec == "region") {
    if (k == "country") { if (v.empty() || (v.size() == 2 && isupper((unsigned char)v[0]) && isupper((unsigned char)v[1]))) cfg.country = v; }
    else if (k == "antenna") cfg.antExt = v == "1" ? 1 : 0;
    else if (k == "gain") cfg.antGain = clampL(viewToLong(v), 0, 15);
    else if (k == "txpower") cfg.txDbm = clampL(viewToLong(v), 0, 20);
    else if (k == "weekstart") cfg.weekStart = v == "1" ? 1 : 0;
    else if (k == "decsep") cfg.decSep = v == "1" ? 1 : 0;
  }
  else if (sec == "ap") {
    if (k == "ssid" && !v.empty() && v.size() <= 32) cfg.apSsid = v;
    else if (k == "pass" && v.size() >= 8 && v.size() <= 63) cfg.apPass = v;
  } else if (sec == "sta") {
    if (k == "enabled") cfg.staEnabled = (v == "1");
    else if (k == "ssid") cfg.staSsid.set(v);
    else if (k == "pass") { if (!v.empty()) cfg.staPass.set(v); }
    else if (k == "dhcp") cfg.staDhcp = (v != "0");
    else if (k == "ip") cfg.ip.set(v); else if (k == "mask") cfg.mask.set(v);
    else if (k == "gw") cfg.gw.set(v); else if (k == "dns1") cfg.dns1.set(v);
    else if (k == "dns2") cfg.dns2.set(v);
  } else if (sec == "auth") {
    if (k == "salt" && !v.empty()) cfg.authSalt.set(v);          // file vecchi (1.7.0)
    else if (k == "hash" && !v.empty()) cfg.authHash.set(v);
    else if (k == "serial") cfg.serialAuth = (v != "0");
    else if (k == "serialidle") cfg.serialIdleMin = clampL(viewToLong(v), 0, 1440);   // minuti di inattivita sulla seriale (0 = mai)
    else if (k == "apqr") cfg.apQr = (v != "0");
    else if (k == "banfails") cfg.banFails = clampL(viewToLong(v), 3, 20);
    else if (k == "bansecs") cfg.banSecs = clampL(viewToLong(v), 10, 3600);
    else if (k == "powbits") { long n = viewToLong(v); cfg.powBits = (n == 0 || (n >= 8 && n <= 20)) ? n : 14; }
    else if (k == "mfanotime") cfg.mfaNoTime = clampL(viewToLong(v), 0, 2);
  } else if (sec.substr(0, 4) == "user") {
    int i = viewToLong(sec.substr(4));
    if (i < 0 || i >= VOS_MAX_USERS || sec.size() > 5) return;
    VosUser& u = cfg.users[i];
    if (k == "name") { if (v.size() >= 3 && v.size() <= 20) u.name = v; }
    else if (k == "role") u.role = clampL(viewToLong(v), 0, 2);
    else if (k == "on") u.on = (v == "1");
    else if (k == "salt" && !v.empty()) u.salt.set(v);
    else if (k == "hash" && hexOnly(v, 64)) u.hash = v;
    else if (k == "mfaon") u.mfaOn = (v == "1");
    else if (k == "mfa" && (v.empty() || hexOnly(v, 40))) u.mfa = v;
    else if (k == "mfalast") u.mfaLast = (uint32_t)strtoul(v.data(), nullptr, 10);
    else if (k == "rec" && v.size() <= 8 * 17) u.rec = v;
  } else if (sec == "firewall") {
    if (k == "mode") cfg.fwMode = clampL(viewToLong(v), 0, 3);
    else if (k == "ntp") cfg.fwNtp = (v == "1");
    else if (k.substr(0, 4) == "rule" && cfg.fwN < VOS_FW_MAX) {
      size_t p1 = v.find('|');
      size_t p2 = p1 != string_view::npos ? v.find('|', p1 + 1) : string_view::npos;
      FwRule r;
      if (p1 == string_view::npos || !cfgParseRange(v.substr(0, p1), r.a, r.b)) return;
      r.on = p2 != string_view::npos ? v.substr(p1 + 1, p2 - p1 - 1) == "1" : true;
      if (p2 != string_view::npos) fwRuleSetName(r, v.data() + p2 + 1, v.size() - p2 - 1);
      cfg.fw[cfg.fwN++] = r;
    }
  } else if (sec == "led") {
    if (k == "mode") cfg.ledMode = clampL(viewToLong(v), 0, LED_MODE_COUNT - 1);
    else if (k == "color") cfg.ledColor = (uint32_t)strtoul(v.data(), NULL, 10) & 0xFFFFFF;
    else if (k == "brightness") cfg.ledBrightness = clampL(viewToLong(v), 0, 255);
    else if (k == "pin") { int p = viewToLong(v); if (p >= 0 && p <= VOS_GPIO_MAX) cfg.ledPin = p; }
    else if (k == "cputhr") cfg.ledCpuThr = clampL(viewToLong(v), 10, 95);
    else {
      for (int i = 0; i < LEDSRC_COUNT; i++)
        if (k == kLedSrcKey[i]) cfg.ledSrcColor[i] = (uint32_t)strtoul(v.data(), NULL, 10) & 0xFFFFFF;
    }
  } else if (sec == "led2") {
    // LED aggiuntivo tolto nella 1.7.0: la sezione dei file vecchi si ignora
  } else if (sec == "mqtt") {
    if (k == "auto") cfg.mqttAuto = (v == "1");
    else if (k == "host") cfg.mqttHost.set(v);
    else if (k == "port") { long p = viewToLong(v); if (p > 0 && p < 65536) cfg.mqttPort = p; }
    else if (k == "tls") cfg.mqttTls = (v == "1");
    else if (k == "user") cfg.mqttUser.set(v);
    else if (k == "pass") { if (!v.empty()) cfg.mqttPass.set(v); }
    else if (k == "prefix") cfg.mqttPrefix.set(v);
    else if (k == "every") cfg.mqttEvery = clampL(viewToLong(v), 5, 3600);
    else if (k == "ha") cfg.mqttHa = (v == "1");
  } else if (sec == "mesh") {
    if (k == "auto") cfg.meshAuto = (v == "1");
    else if (k == "role") cfg.meshRole = clampL(viewToLong(v), 0, 2);
    else if (k == "channel") cfg.meshCh = clampL(viewToLong(v), 1, 13);
    else if (k == "key" && hexOnly(v, 64)) cfg.meshKey = v;
  } else if (sec == "ble") {
    if (k == "on") cfg.bleOn = (v == "1");
  } else if (sec == "airplane") {
    if (k == "on") cfg.airOn = (v == "1");
    else if (k == "exit") cfg.airExit = clampL(viewToLong(v), 0, 3);
    else if (k == "until") cfg.airUntil = strtoul(v.data(), NULL, 10);
    else if (k == "at") cfg.airAt = clampL(viewToLong(v), 0, 1439);
  } else if (sec == "time") {
    if (k == "ntp") cfg.ntpOn = (v == "1");
    else if (k == "ntpauto") cfg.ntpAutoOff = (v == "1");
    else if (k == "serve") cfg.ntpServe = (v == "1");
    else if (k == "every") { long m = viewToLong(v); if (m >= 0 && m <= 10080) cfg.ntpEvery = m; }
    else if (k == "server" && !v.empty()) cfg.ntpServer.set(v);
    else if (k == "server2") cfg.ntpServer2.set(v);
    else if (k == "tz" && !v.empty()) cfg.tz.set(v);
    else if (k == "tzname" && !v.empty()) cfg.tzName.set(v);
    else if (k == "datefmt") cfg.dateFmt = clampL(viewToLong(v), 0, 2);
    else if (k == "timefmt") cfg.timeFmt = clampL(viewToLong(v), 0, 1);
    else if (k == "tempunit") cfg.tempUnit = clampL(viewToLong(v), 0, 1);
  } else if (sec == "watchdog") {
    if (k == "task") cfg.wdTask = (v != "0");
    else if (k == "net") cfg.wdNet = (v != "0");
    else if (k == "ram") cfg.wdRam = (v != "0");
    else if (k == "netmin") cfg.wdNetMin = clampL(viewToLong(v), 2, 1440);
    else if (k == "ramkb") cfg.wdRamKb = clampL(viewToLong(v), 8, 200);
    else if (k == "updays") cfg.wdUpDays = clampL(viewToLong(v), 0, 365);
    else if (k == "at") cfg.wdAt = clampL(viewToLong(v), -1, 1439);
    else if (k == "days") cfg.wdDays = viewToLong(v) & 0x7F;
    else if (k == "twdt") { long t = viewToLong(v); if (wdTwdtOk(t)) cfg.wdTwdt = (uint8_t)t; }   // fuori intervallo: resta il valore di base
    else if (k.rfind("lim_", 0) == 0) {
      char name[16] = "";
      string_view nm = k.substr(4);
      if (nm.size() < sizeof name) { memcpy(name, nm.data(), nm.size()); name[nm.size()] = 0; }
      int i = wdLimIndex(name); long n = viewToLong(v);
      if (wdLimOk(i, n)) cfg.wdLim[i] = (uint16_t)n;
    }
  } else if (sec == "sd") {
    if (k == "enabled") cfg.sdEnabled = (v == "1");
    else if (k == "cs") cfg.sdCs = viewToLong(v); else if (k == "sck") cfg.sdSck = viewToLong(v);
    else if (k == "miso") cfg.sdMiso = viewToLong(v); else if (k == "mosi") cfg.sdMosi = viewToLong(v);
  }
}

// Legge un valore dopo la chiave: tra apici con escape '\'' oppure senza apici (fino a fine riga, senza spazi ai lati).
// Scrive in out (max cap - 1 caratteri) e chiude con 0; i '\r' si saltano. Ritorna false se manca il valore o l'apice finale.
// tooLong = il valore non entra in out.
static bool parseQuoted(string_view line, size_t pos, char* out, size_t cap, bool& tooLong) {
  size_t n = 0;
  tooLong = false;
  out[0] = 0;
  auto put = [&](char c) { if (c == '\r') return; if (n + 1 < cap) out[n++] = c; else tooLong = true; };
  while (pos < line.size() && (line[pos] == ' ' || line[pos] == '\t')) pos++;
  if (pos >= line.size()) return false;
  if (line[pos] != '\'') {                       // senza apici: fino a fine riga
    string_view rest = trimView(line.substr(pos));
    for (char c : rest) put(c);
    out[n] = 0;
    return true;
  }
  pos++;
  while (pos < line.size()) {
    char c = line[pos];
    if (c == '\'') {
      if (line.substr(pos + 1, 3) == "\\''") { put('\''); pos += 4; continue; }
      out[n] = 0;
      return true;
    }
    put(c); pos++;
  }
  out[n] = 0;
  return false;
}

bool cfgImport(const char* text, size_t len, ErrBuf& err) {
  char sec[32] = "";
  char val[CFG_VAL_MAX], tmp[32];
  int ln = 0, applied = 0;
  size_t i = 0;
  bool fwSeen = false, tooLong;
  string_view all(text, len);
  while (i <= len) {
    size_t e = all.find('\n', i);
    if (e == string_view::npos) e = len;
    string_view t = trimView(all.substr(i, e - i));
    i = e + 1; ln++;
    if (t.empty() || t[0] == '#') continue;
    if (t.substr(0, 7) == "config ") {
      string_view rest = trimView(t.substr(7));
      size_t sp = rest.find(' ');
      bool named = false;
      if (sp != string_view::npos) { parseQuoted(rest, sp + 1, tmp, sizeof tmp, tooLong); named = tmp[0] && !tooLong; }
      string_view name = named ? string_view(tmp) : rest;
      if (name.size() >= sizeof sec) name = string_view();          // nome troppo lungo: non e nessuna sezione nostra
      memcpy(sec, name.data(), name.size()); sec[name.size()] = 0;
      if (!strcmp(sec, "firewall") && !fwSeen) { fwSeen = true; cfg.fwN = 0; }   // la lista delle regole si sostituisce
      continue;
    }
    if (t.substr(0, 7) == "option ") {
      string_view rest = trimView(t.substr(7));
      size_t sp = rest.find(' ');
      if (sp == string_view::npos) { err.setf(tr("riga %d: option senza valore"), ln); return false; }
      if (!parseQuoted(rest, sp + 1, val, sizeof val, tooLong)) { err.setf(tr("riga %d: apici non chiusi"), ln); return false; }
      if (!tooLong) applyKey(sec, rest.substr(0, sp), string_view(val));
      applied++;
      continue;
    }
    err.setf(tr("riga %d: non capisco"), ln);
    return false;
  }
  if (applied == 0) { err = tr("nessuna opzione trovata"); return false; }
  return true;
}
