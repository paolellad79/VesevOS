// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_config_export.cpp
#include "vos_config.h"
#include "vos_i18n.h"
#include "vos_util.h"
#include "vos_log.h"
#include "vos_audit.h"
#include "../drivers/vos_drv_fs.h"
#include <Preferences.h>
#include "vos_config_int.h"

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
  opt(s, "ntpauto", cfg.ntpAutoOff ? "1" : "0");
  opt(s, "server", cfg.ntpServer);
  opt(s, "server2", cfg.ntpServer2);
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
