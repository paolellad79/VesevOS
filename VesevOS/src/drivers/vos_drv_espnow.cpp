// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_drv_espnow.cpp
#include "vos_drv_espnow.h"
#if VOS_WITH_MESH
#include <WiFi.h>
#include <esp_wifi.h>
#include <esp_now.h>
#include <esp_mac.h>

static EspNowRxFn s_rx = nullptr;
static wifi_interface_t s_if = (wifi_interface_t)99;
static const uint8_t ALL[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

bool drvEspNowRadioUp() { return WiFi.getMode() != WIFI_MODE_NULL; }
bool drvEspNowIsAp() { return WiFi.getMode() == WIFI_MODE_AP; }
int  drvEspNowChannel() { uint8_t ch = 0; wifi_second_chan_t s; if (esp_wifi_get_channel(&ch, &s) != ESP_OK) return 0; return ch; }
void drvEspNowMac(uint8_t out[6]) { esp_read_mac(out, drvEspNowIsAp() ? ESP_MAC_WIFI_SOFTAP : ESP_MAC_WIFI_STA); }

static void onRecv(const esp_now_recv_info_t* info, const uint8_t* data, int len) {
  if (s_rx) s_rx(data, len, info && info->rx_ctrl ? info->rx_ctrl->rssi : 0);
}

static void peerEnsure() {
  wifi_interface_t want = drvEspNowIsAp() ? WIFI_IF_AP : WIFI_IF_STA;
  if (esp_now_is_peer_exist(ALL) && want == s_if) return;
  if (esp_now_is_peer_exist(ALL)) esp_now_del_peer(ALL);
  esp_now_peer_info_t p; memset(&p, 0, sizeof(p));
  memcpy(p.peer_addr, ALL, 6); p.channel = 0; p.ifidx = want; p.encrypt = false;
  esp_now_add_peer(&p);
  s_if = want;
}

bool drvEspNowBegin(EspNowRxFn onRx) {
  if (esp_now_init() != ESP_OK) return false;
  s_rx = onRx;
  esp_now_register_recv_cb(onRecv);
  s_if = (wifi_interface_t)99;
  peerEnsure();
  return true;
}

void drvEspNowEnd() {
  esp_now_unregister_recv_cb();
  esp_now_deinit();
  s_rx = nullptr;
}

bool drvEspNowSend(const uint8_t* buf, size_t len) {
  peerEnsure();
  return esp_now_send(ALL, buf, len) == ESP_OK;
}
#endif
