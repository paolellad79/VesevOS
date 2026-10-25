// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_config_export.cpp
#include "vos_config.h"
#include <stdio.h>
#include "vos_config_int.h"

// Scrive le righe "\toption chiave 'valore'" in un HeapBuf. Se manca la memoria o si supera il tetto l'HeapBuf lo ricorda
// (oom()/over()): cfgExport lo controlla una volta sola alla fine.
class Out {
 public:
  explicit Out(HeapBuf& b) : b_(b) {}
  void raw(const char* t) { b_.add(t); }
  void section(const char* type, const char* name) { b_.addf("\nconfig %s '%s'\n", type, name); }
  void opt(const char* k, const char* v) {                // 'valore' con escape OpenWrt
    b_.addf("\toption %s '", k);
    for (const char* p = v; *p; p++) {
      if (*p == '\'') b_.add("'\\''"); else b_.add(*p);
    }
    b_.add("'\n");
  }
  void flag(const char* k, bool v) { opt(k, v ? "1" : "0"); }
  void num(const char* k, long v) { char t[16]; snprintf(t, sizeof t, "%ld", v); opt(k, t); }
  void unum(const char* k, unsigned long v) { char t[16]; snprintf(t, sizeof t, "%lu", v); opt(k, t); }
 private:
  HeapBuf& b_;
};

// "1.2.3.4" (ordine umano: primo numero nel byte alto); a-b se l'intervallo ha due indirizzi
static void ipText(uint32_t ip, char out[16]) {
  snprintf(out, 16, "%u.%u.%u.%u", (unsigned)(ip >> 24), (unsigned)((ip >> 16) & 255), (unsigned)((ip >> 8) & 255), (unsigned)(ip & 255));
}
static void fwRuleText(const FwRule& r, char* out, size_t cap) {
  char a[16], b[16];
  ipText(r.a, a); ipText(r.b, b);
  if (r.a == r.b) snprintf(out, cap, "%s|%c|%s", a, r.on ? '1' : '0', r.name);
  else            snprintf(out, cap, "%s-%s|%c|%s", a, b, r.on ? '1' : '0', r.name);
}

bool cfgExport(HeapBuf& out, bool withSecrets) {
  Out s(out);
  auto secret = [&](const char* v) { return withSecrets ? v : ""; };    // senza segreti il valore resta vuoto
  s.raw("# VesevOS config\n");
  s.raw("config system 'system'\n");
  s.opt("hostname", cfg.hostname.c_str());
  s.opt("domain", cfg.domain.c_str());
  s.num("cpu", cfg.cpuMhz);
  // governor e parametri solo se diversi dal predefinito: chi non li usa ottiene lo stesso file di prima
  if (cfg.cpuGov != 0) s.num("cpugov", cfg.cpuGov);
  if (cfg.cpuUp != 80) s.num("cpuup", cfg.cpuUp);
  if (cfg.cpuDown != 20) s.num("cpudown", cfg.cpuDown);
  if (cfg.cpuHold != 10) s.num("cpuhold", cfg.cpuHold);
  s.flag("stat", cfg.statOn);
  s.num("loglv", cfg.logLevel);
  s.num("btn1", cfg.btnAct[0]);
  s.num("btn2", cfg.btnAct[1]);
  s.num("btn3", cfg.btnAct[2]);
  s.unum("serbaud", cfg.serBaud);
  s.num("sereol", cfg.serEol);
  s.num("sertx", cfg.serTx);
  s.flag("serecho", cfg.serEcho);
  s.flag("serin", cfg.serIn);
  s.flag("serlog", cfg.serLogOut);
  s.flag("serban", cfg.serBanner);
  s.num("pwmode", cfg.pwMode);
  s.num("pwawake", cfg.pwAwake);
  s.num("pwsleep", cfg.pwSleep);
  s.opt("lang", cfg.lang.c_str());
  s.flag("setup", cfg.setupDone);
  s.flag("https", cfg.https);
  s.section("svc", "svc");
  s.flag("apon", cfg.apOn);
  s.flag("captive", cfg.apCaptive);
  s.flag("dhcp", cfg.dhcpOn);
  s.num("lease", cfg.dhcpLease);
  s.flag("mdns", cfg.mdnsOn);
  s.flag("wps", cfg.wpsOn);
  s.flag("wpspin", cfg.wpsPin);
  s.flag("httpon", cfg.httpOn);
  s.flag("redirect", cfg.httpRedirect);
  s.num("httpport", cfg.httpPort);
  s.num("httpsport", cfg.httpsPort);
  s.section("region", "region");
  s.opt("country", cfg.country.c_str());
  s.num("antenna", cfg.antExt);
  s.num("gain", cfg.antGain);
  s.num("txpower", cfg.txDbm);
  s.num("weekstart", cfg.weekStart);
  s.num("decsep", cfg.decSep);
  s.section("ap", "ap");
  s.opt("ssid", cfg.apSsid.c_str());
  s.opt("pass", secret(cfg.apPass.c_str()));
  s.section("client", "sta");
  s.flag("enabled", cfg.staEnabled);
  s.opt("ssid", cfg.staSsid.c_str());
  s.opt("pass", secret(cfg.staPass.c_str()));
  s.flag("dhcp", cfg.staDhcp);
  s.opt("ip", cfg.ip.c_str()); s.opt("mask", cfg.mask.c_str()); s.opt("gw", cfg.gw.c_str());
  s.opt("dns1", cfg.dns1.c_str()); s.opt("dns2", cfg.dns2.c_str());
  s.section("auth", "auth");
  s.flag("serial", cfg.serialAuth);
  s.flag("apqr", cfg.apQr);
  s.num("banfails", cfg.banFails);
  s.unum("bansecs", cfg.banSecs);
  s.num("powbits", cfg.powBits);
  s.num("mfanotime", cfg.mfaNoTime);
  if (cfg.serialIdleMin != 0) s.num("serialidle", cfg.serialIdleMin);   // scritta solo se diversa dal predefinito (0)
  for (int i = 0; i < VOS_MAX_USERS; i++) {
    const VosUser& u = cfg.users[i];
    if (u.name.empty()) continue;
    char id[12];
    snprintf(id, sizeof id, "user%d", i);
    s.section("user", id);
    s.opt("name", u.name.c_str());
    s.num("role", u.role);
    s.flag("on", u.on);
    s.opt("salt", secret(u.salt.c_str()));
    s.opt("hash", secret(u.hash.c_str()));
    s.flag("mfaon", u.mfaOn);
    s.opt("mfa", secret(u.mfa.c_str()));
    s.unum("mfalast", u.mfaLast);
    s.opt("rec", secret(u.rec.c_str()));
  }
  s.section("firewall", "firewall");
  s.num("mode", cfg.fwMode);
  s.flag("ntp", cfg.fwNtp);
  for (int i = 0; i < cfg.fwN && i < VOS_FW_MAX; i++) {
    char key[12], rule[16 + 16 + 4 + FW_NAME_MAX + 4];
    snprintf(key, sizeof key, "rule%d", i);
    fwRuleText(cfg.fw[i], rule, sizeof rule);
    s.opt(key, rule);
  }
  s.section("led", "led");
  s.num("mode", cfg.ledMode);
  s.unum("color", cfg.ledColor);
  s.num("brightness", cfg.ledBrightness);
  s.num("pin", cfg.ledPin);
  // colori per sorgente e soglia della CPU solo se diversi dal predefinito: chi non li cambia ottiene lo stesso file di prima
  for (int i = 0; i < LEDSRC_COUNT; i++)
    if (cfg.ledSrcColor[i] != kLedSrcDefault[i]) s.unum(kLedSrcKey[i], cfg.ledSrcColor[i]);
  if (cfg.ledCpuThr != LED_CPU_THR_DEFAULT) s.num("cputhr", cfg.ledCpuThr);
  s.section("mqtt", "mqtt");
  s.flag("auto", cfg.mqttAuto);
  s.opt("host", cfg.mqttHost.c_str());
  s.num("port", cfg.mqttPort);
  s.flag("tls", cfg.mqttTls);
  s.opt("user", cfg.mqttUser.c_str());
  s.opt("pass", secret(cfg.mqttPass.c_str()));
  s.opt("prefix", cfg.mqttPrefix.c_str());
  s.num("every", cfg.mqttEvery);
  s.flag("ha", cfg.mqttHa);
  s.section("mesh", "mesh");
  s.flag("auto", cfg.meshAuto);
  s.num("role", cfg.meshRole);
  s.num("channel", cfg.meshCh);
  s.opt("key", secret(cfg.meshKey.c_str()));
  s.section("ble", "ble");
  s.flag("on", cfg.bleOn);
  s.section("airplane", "airplane");
  s.num("on", cfg.airOn);
  s.num("exit", cfg.airExit);
  s.unum("until", cfg.airUntil);
  s.num("at", cfg.airAt);
  s.section("time", "time");
  s.flag("ntp", cfg.ntpOn);
  s.flag("ntpauto", cfg.ntpAutoOff);
  s.opt("server", cfg.ntpServer.c_str());
  s.opt("server2", cfg.ntpServer2.c_str());
  s.flag("serve", cfg.ntpServe);
  s.unum("every", cfg.ntpEvery);
  s.opt("tz", cfg.tz.c_str());
  s.opt("tzname", cfg.tzName.c_str());
  s.num("datefmt", cfg.dateFmt);
  s.num("timefmt", cfg.timeFmt);
  s.num("tempunit", cfg.tempUnit);
  s.section("watchdog", "watchdog");
  s.flag("task", cfg.wdTask);
  s.flag("net", cfg.wdNet);
  s.flag("ram", cfg.wdRam);
  s.num("netmin", cfg.wdNetMin);
  s.num("ramkb", cfg.wdRamKb);
  s.num("updays", cfg.wdUpDays);
  s.num("at", cfg.wdAt);
  s.num("days", cfg.wdDays);
  // tempi dei watchdog solo se diversi dal predefinito: chi non li cambia ottiene lo stesso file di prima
  if (cfg.wdTwdt != WD_TWDT_DEF) s.num("twdt", cfg.wdTwdt);
  for (int i = 0; i < WD_LIM_N; i++) {
    if (cfg.wdLim[i] == WD_LIMITS[i].def) continue;
    char key[16]; snprintf(key, sizeof key, "lim_%s", WD_LIMITS[i].name);
    s.num(key, cfg.wdLim[i]);
  }
  s.section("pins", "pins");
  for (int i = 0; i < VOS_GPIO_COUNT; i++) {
    if (cfg.pinNote[i].empty()) continue;
    char key[8];
    snprintf(key, sizeof key, "n%d", i);
    s.opt(key, cfg.pinNote[i].c_str());
  }
  s.section("sd", "sd");
  s.flag("enabled", cfg.sdEnabled);
  s.num("cs", cfg.sdCs); s.num("sck", cfg.sdSck);
  s.num("miso", cfg.sdMiso); s.num("mosi", cfg.sdMosi);
  return !out.oom() && !out.over();
}
