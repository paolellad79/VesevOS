// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_drv_wps.cpp
#include "vos_drv_wps.h"

#if VOS_WITH_WPS && __has_include(<esp_wps.h>)
#include <WiFi.h>
#include <esp_wifi.h>
#include <esp_wps.h>

static volatile uint8_t g_ev = DWE_NONE;
static char g_pin[9] = {0};
static bool g_hooked = false;

// Chiamata dal task degli eventi Wi-Fi: solo copie, niente altro.
static void onWifiEvent(WiFiEvent_t event, arduino_event_info_t info) {
  switch (event) {
    case ARDUINO_EVENT_WPS_ER_SUCCESS: g_ev = DWE_OK; break;
    case ARDUINO_EVENT_WPS_ER_FAILED: g_ev = DWE_FAILED; break;
    case ARDUINO_EVENT_WPS_ER_TIMEOUT: g_ev = DWE_TIMEOUT; break;
    case ARDUINO_EVENT_WPS_ER_PIN:
      for (int i = 0; i < 8; i++) { uint8_t d = info.wps_er_pin.pin_code[i]; g_pin[i] = (d >= '0' && d <= '9') ? (char)d : (char)('0' + (d % 10)); }
      g_pin[8] = 0;
      break;
    default: break;
  }
}

bool drvWpsAvailable() { return true; }

bool drvWpsStart(bool pin, DrvWpsErr& err) {
  err = DWERR_NONE;
  if (!g_hooked) { WiFi.onEvent(onWifiEvent); g_hooked = true; }
  g_ev = DWE_NONE; g_pin[0] = 0;
  esp_wps_config_t cfg = WPS_CONFIG_INIT_DEFAULT(pin ? WPS_TYPE_PIN : WPS_TYPE_PBC);
  if (esp_wifi_wps_enable(&cfg) != ESP_OK) { err = DWERR_START; return false; }
  if (esp_wifi_wps_start(0) != ESP_OK) { esp_wifi_wps_disable(); err = DWERR_START; return false; }
  return true;
}

void drvWpsStop() { esp_wifi_wps_disable(); g_pin[0] = 0; }
DrvWpsEvent drvWpsPoll() { DrvWpsEvent e = (DrvWpsEvent)g_ev; g_ev = DWE_NONE; return e; }
String drvWpsPin() { return String(g_pin); }

bool drvWpsCredentials(String& ssid, String& pass) {
  wifi_config_t wc; memset(&wc, 0, sizeof(wc));
  if (esp_wifi_get_config(WIFI_IF_STA, &wc) != ESP_OK) return false;
  char s[33], p[65];
  memcpy(s, wc.sta.ssid, 32); s[32] = 0;
  memcpy(p, wc.sta.password, 64); p[64] = 0;
  ssid = s; pass = p;
  return ssid.length() > 0;
}

#else   // WPS tolto (VOS_WITH_WPS 0) o librerie senza WPS: il resto del firmware funziona uguale

bool drvWpsAvailable() { return false; }
bool drvWpsStart(bool, DrvWpsErr& err) { err = DWERR_ABSENT; return false; }
void drvWpsStop() {}
DrvWpsEvent drvWpsPoll() { return DWE_NONE; }
String drvWpsPin() { return String(); }
bool drvWpsCredentials(String&, String&) { return false; }
#endif
