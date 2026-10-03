// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// VesevOS - vos_net.cpp
#include "vos_net.h"
#include "vos_i18n.h"
#include "vos_config.h"
#include "vos_util.h"
#include "vos_log.h"
#include "vos_led.h"
#include "vos_web.h"
#include <WiFi.h>
#include <ESPmDNS.h>

static volatile NetState g_state = NET_BOOT;
static volatile bool g_reconf = true;
static volatile bool g_scanReq = false;
static volatile bool g_scanRun = false;
static String g_scanJson = "[]";

NetState netState() { return g_state; }
void netReconfigure() { g_reconf = true; }
void netScanStart() { g_scanReq = true; }

static void setState(NetState s) { g_state = s; ledSetNetState(s); }

String netIpString() {
  if (WiFi.getMode() == WIFI_AP) return WiFi.softAPIP().toString();
  return WiFi.localIP().toString();
}

String netStatusJson() {
  String mode = (g_state == NET_AP) ? String("AP") : (g_state == NET_CLIENT_OK ? String("Client") : (g_state == NET_CLIENT_TRY ? String(tr("Connessione...")) : String(tr("Avvio"))));
  String j = "{\"mode\":\"" + mode + "\"";
  j += ",\"ip\":\"" + netIpString() + "\"";
  j += ",\"host\":\"" + jsonEscape(cfg.hostname) + "\"";
  j += ",\"fqdn\":\"" + jsonEscape(cfg.domain.length() ? cfg.hostname + "." + cfg.domain : cfg.hostname) + "\"";
  if (g_state == NET_CLIENT_OK) {
    j += ",\"ssid\":\"" + jsonEscape(WiFi.SSID()) + "\"";
    j += ",\"rssi\":" + String(WiFi.RSSI());
    j += ",\"gw\":\"" + WiFi.gatewayIP().toString() + "\"";
    j += ",\"mask\":\"" + WiFi.subnetMask().toString() + "\"";
    j += ",\"dns\":\"" + WiFi.dnsIP().toString() + "\"";
  } else {
    j += ",\"ssid\":\"" + jsonEscape(cfg.apSsid) + "\"";
  }
  j += ",\"mac\":\"" + WiFi.macAddress() + "\"";
  j += "}";
  return j;
}

String netScanJson() {
  return "{\"running\":" + String(g_scanRun ? "true" : "false") + ",\"list\":" + g_scanJson + "}";
}

static void startAp() {
  WiFi.disconnect(true);
  WiFi.mode(WIFI_AP);
  IPAddress ip(192, 168, 4, 1), gw(192, 168, 4, 1), nm(255, 255, 255, 0);
  WiFi.softAPConfig(ip, gw, nm);
  WiFi.softAP(cfg.apSsid.c_str(), cfg.apPass.c_str());
  setState(NET_AP);
  vlog("NET: AP '%s' su 192.168.4.1", cfg.apSsid.c_str());
}

static bool applyStaticIp() {
  if (cfg.staDhcp) return true;
  uint32_t ip, mk, gw, d1 = 0, d2 = 0;
  if (!ipParse(cfg.ip, ip) || !ipParse(cfg.mask, mk) || !maskValid(mk) || !ipParse(cfg.gw, gw)) {
    vlog("NET: IP statico non valido, uso DHCP");
    return false;
  }
  ipParse(cfg.dns1, d1); ipParse(cfg.dns2, d2);
  auto toIP = [](uint32_t v) { return IPAddress((v >> 24) & 255, (v >> 16) & 255, (v >> 8) & 255, v & 255); };
  WiFi.config(toIP(ip), toIP(gw), toIP(mk), toIP(d1), toIP(d2));
  return true;
}

static bool tryClient() {
  setState(NET_CLIENT_TRY);
  WiFi.disconnect(true);
  delay(200);
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(cfg.hostname.c_str());
  if (!applyStaticIp()) WiFi.config(IPAddress(), IPAddress(), IPAddress());  // torna a DHCP
  WiFi.begin(cfg.staSsid.c_str(), cfg.staPass.c_str());
  for (int i = 0; i < 40; i++) {            // max 20 s
    if (WiFi.status() == WL_CONNECTED) {
      setState(NET_CLIENT_OK);
      vlog("NET: client connesso, IP %s", WiFi.localIP().toString().c_str());
      return true;
    }
    vTaskDelay(pdMS_TO_TICKS(500));
  }
  return false;
}

static void doScan() {
  g_scanRun = true;
  wifi_mode_t m = WiFi.getMode();
  if (m == WIFI_AP) WiFi.mode(WIFI_AP_STA);
  int n = WiFi.scanNetworks(false, true);
  String j = "[";
  for (int i = 0; i < n; i++) {
    if (i) j += ",";
    j += "{\"ssid\":\"" + jsonEscape(WiFi.SSID(i)) + "\",\"rssi\":" + String(WiFi.RSSI(i)) +
         ",\"ch\":" + String(WiFi.channel(i)) +
         ",\"open\":" + String(WiFi.encryptionType(i) == WIFI_AUTH_OPEN ? "true" : "false") + "}";
  }
  j += "]";
  g_scanJson = j;
  WiFi.scanDelete();
  if (m == WIFI_AP) WiFi.mode(WIFI_AP);
  g_scanRun = false;
}

static void netTask(void*) {
  int fails = 0;
  for (;;) {
    if (g_reconf) {
      g_reconf = false; fails = 0;
      if (cfg.staEnabled && cfg.staSsid.length()) {
        if (!tryClient()) startAp();     // se fallisce torna AP
      } else startAp();
      MDNS.end();
      if (MDNS.begin(cfg.hostname.c_str())) MDNS.addService("http", "tcp", 80);
      webStart();                       // il server parte solo ora che la rete e pronta
    }
    if (g_scanReq) { g_scanReq = false; doScan(); }
    if (cfg.staEnabled && g_state == NET_CLIENT_OK && WiFi.status() != WL_CONNECTED) {
      vlog("NET: connessione persa");
      setState(NET_CLIENT_TRY);
    }
    if (g_state == NET_CLIENT_TRY) {
      if (WiFi.status() == WL_CONNECTED) setState(NET_CLIENT_OK);
      else if (++fails >= 60) { fails = 0; vlog("NET: troppi tentativi, torno in AP"); startAp(); }
    } else fails = 0;
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

void netInit() {
  g_reconf = true;
  xTaskCreatePinnedToCore(netTask, "net", 6144, NULL, 2, NULL, 0);
}
