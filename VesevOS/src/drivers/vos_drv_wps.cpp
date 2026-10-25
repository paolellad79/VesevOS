// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_drv_wps.cpp
#include "vos_drv_wps.h"

#if VOS_WITH_WPS && __has_include(<esp_wps.h>)
#include <esp_wifi.h>
#include <esp_wps.h>
#include <esp_event.h>

static volatile uint8_t g_ev = DWE_NONE;
static char g_pin[9] = {0};
static bool g_hooked = false;

// Chiamata dal task degli eventi Wi-Fi: solo copie, niente altro.
static void onWifiEvent(void*, esp_event_base_t base, int32_t id, void* data) {
  if (base != WIFI_EVENT) return;
  switch (id) {
    case WIFI_EVENT_STA_WPS_ER_SUCCESS: g_ev = DWE_OK; break;
    case WIFI_EVENT_STA_WPS_ER_FAILED: g_ev = DWE_FAILED; break;
    case WIFI_EVENT_STA_WPS_ER_TIMEOUT: g_ev = DWE_TIMEOUT; break;
    case WIFI_EVENT_STA_WPS_ER_PIN:
      if (!data) break;
      for (int i = 0; i < 8; i++) { uint8_t d = ((wifi_event_sta_wps_er_pin_t*)data)->pin_code[i]; g_pin[i] = (d >= '0' && d <= '9') ? (char)d : (char)('0' + (d % 10)); }
      g_pin[8] = 0;
      break;
    default: break;
  }
}

bool drvWpsAvailable() { return true; }

bool drvWpsStart(bool pin, DrvWpsErr& err) {
  err = DWERR_NONE;
  if (!g_hooked) g_hooked = esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, onWifiEvent, nullptr) == ESP_OK;
  g_ev = DWE_NONE; g_pin[0] = 0;
  esp_wps_config_t cfg = WPS_CONFIG_INIT_DEFAULT(pin ? WPS_TYPE_PIN : WPS_TYPE_PBC);
  if (esp_wifi_wps_enable(&cfg) != ESP_OK) { err = DWERR_START; return false; }
  if (esp_wifi_wps_start(0) != ESP_OK) { esp_wifi_wps_disable(); err = DWERR_START; return false; }
  return true;
}

void drvWpsStop() { esp_wifi_wps_disable(); g_pin[0] = 0; }
DrvWpsEvent drvWpsPoll() { DrvWpsEvent e = (DrvWpsEvent)g_ev; g_ev = DWE_NONE; return e; }
void drvWpsPin(char* out, size_t n) { snprintf(out, n, "%s", g_pin); }

bool drvWpsCredentials(char* ssid, size_t ns, char* pass, size_t np) {
  if (ns < 33 || np < 65) return false;
  wifi_config_t wc; memset(&wc, 0, sizeof(wc));
  if (esp_wifi_get_config(WIFI_IF_STA, &wc) != ESP_OK) return false;
  memcpy(ssid, wc.sta.ssid, 32); ssid[32] = 0;
  memcpy(pass, wc.sta.password, 64); pass[64] = 0;
  return ssid[0] != 0;
}

#else   // WPS tolto (VOS_WITH_WPS 0) o librerie senza WPS: il resto del firmware funziona uguale

bool drvWpsAvailable() { return false; }
bool drvWpsStart(bool, DrvWpsErr& err) { err = DWERR_ABSENT; return false; }
void drvWpsStop() {}
DrvWpsEvent drvWpsPoll() { return DWE_NONE; }
void drvWpsPin(char* out, size_t n) { if (n) out[0] = 0; }
bool drvWpsCredentials(char*, size_t, char*, size_t) { return false; }
#endif
