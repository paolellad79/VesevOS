// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_drv_wifi.cpp
#include "vos_drv_wifi.h"
#include <WiFi.h>
#include <esp_wifi.h>

static uint32_t toNum(IPAddress a) { return ((uint32_t)a[0] << 24) | ((uint32_t)a[1] << 16) | ((uint32_t)a[2] << 8) | (uint32_t)a[3]; }
static IPAddress toIp(uint32_t v) { return IPAddress((v >> 24) & 255, (v >> 16) & 255, (v >> 8) & 255, v & 255); }

DrvWifiMode drvWifiMode() {
  switch (WiFi.getMode()) {
    case WIFI_MODE_STA: return DW_STA;
    case WIFI_MODE_AP: return DW_AP;
    case WIFI_MODE_APSTA: return DW_AP_STA;
    default: return DW_OFF;
  }
}
void drvWifiSetMode(DrvWifiMode m) {
  WiFi.mode(m == DW_STA ? WIFI_STA : m == DW_AP ? WIFI_AP : m == DW_AP_STA ? WIFI_AP_STA : WIFI_OFF);
}
void drvWifiDisconnect(bool wifiOff, bool eraseAp) { WiFi.disconnect(wifiOff, eraseAp); }

void drvWifiSetHostname(const char* hostname) { WiFi.setHostname(hostname); }
void drvWifiStaConfigStatic(uint32_t ip, uint32_t gw, uint32_t mask, uint32_t d1, uint32_t d2) { WiFi.config(toIp(ip), toIp(gw), toIp(mask), toIp(d1), toIp(d2)); }
void drvWifiStaConfigDhcp() { WiFi.config(IPAddress(), IPAddress(), IPAddress()); }
void drvWifiStaBegin(const char* hostname, const char* ssid, const char* pass) {
  if (hostname) WiFi.setHostname(hostname);
  WiFi.begin(ssid, pass);
}
DrvWifiSta drvWifiStaStatus() {
  wl_status_t s = WiFi.status();
  return s == WL_CONNECTED ? DWS_CONNECTED : s == WL_NO_SSID_AVAIL ? DWS_NO_SSID : s == WL_IDLE_STATUS ? DWS_IDLE : DWS_OTHER;
}
String   drvWifiSsid() { return WiFi.SSID(); }
int      drvWifiRssi() { return WiFi.RSSI(); }
uint32_t drvWifiLocalIp() { return toNum(WiFi.localIP()); }
uint32_t drvWifiGateway() { return toNum(WiFi.gatewayIP()); }
uint32_t drvWifiMask() { return toNum(WiFi.subnetMask()); }
uint32_t drvWifiDns(int i) { return toNum(WiFi.dnsIP(i)); }

bool drvWifiApStart(const char* ssid, const char* pass, int channel, uint32_t ip, uint32_t gw, uint32_t mask) {
  WiFi.softAPConfig(toIp(ip), toIp(gw), toIp(mask));
  return WiFi.softAP(ssid, pass, channel);
}
bool     drvWifiApRetune(const char* ssid, const char* pass, int channel) { return WiFi.softAP(ssid, pass, channel); }
uint32_t drvWifiApIp() { return toNum(WiFi.softAPIP()); }
int      drvWifiApClients() { return WiFi.softAPgetStationNum(); }
int      drvWifiChannel() { return WiFi.channel(); }

String drvWifiApplyRegion(const char* ccIn, int maxChannels, int txDbm) {
  if (WiFi.getMode() == WIFI_MODE_NULL) return String();
  String cc = ccIn;
  // false = la scheda NON prende il paese dai router vicini (potrebbero annunciare regole diverse)
  esp_err_t e = esp_wifi_set_country_code(cc.c_str(), false);
  if (e != ESP_OK && cc != "01") { esp_wifi_set_country_code("01", false); cc = "01"; }
  if (cc == "01" || maxChannels == 11) {
    wifi_country_t wc; memset(&wc, 0, sizeof(wc));
    if (esp_wifi_get_country(&wc) == ESP_OK && wc.schan + wc.nchan - 1 > maxChannels) {
      wc.schan = 1; wc.nchan = maxChannels; wc.policy = WIFI_COUNTRY_POLICY_MANUAL;
      esp_wifi_set_country(&wc);
    }
  }
  esp_wifi_set_max_tx_power((int8_t)(txDbm * 4));       // unita di 0,25 dBm
  return cc;
}
void drvWifiPowerSave(bool maxSaving) { esp_wifi_set_ps(maxSaving ? WIFI_PS_MAX_MODEM : WIFI_PS_MIN_MODEM); }
void drvWifiShutdown() { WiFi.disconnect(true); WiFi.mode(WIFI_OFF); }

int    drvWifiScanStart() { return WiFi.scanNetworks(false, true); }
String drvWifiScanSsid(int i) { return WiFi.SSID(i); }
int    drvWifiScanRssi(int i) { return WiFi.RSSI(i); }
int    drvWifiScanChannel(int i) { return WiFi.channel(i); }
bool   drvWifiScanOpen(int i) { return WiFi.encryptionType(i) == WIFI_AUTH_OPEN; }
void   drvWifiScanClear() { WiFi.scanDelete(); }
