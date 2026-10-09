// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_stats.cpp
#include "vos_stats.h"
#include "../core/vos_common.h"
#include "../core/vos_config.h"
#include "../net/vos_net.h"
#include "vos_ble.h"
#include "../core/vos_i18n.h"
#include "../drivers/vos_drv_wifi.h"

#if VOS_WITH_STATS
struct Stats {
  uint32_t t0;                                    // secondi dall'avvio quando e iniziato il conteggio
  uint32_t wifiUp, wifiDown, wifiSec;             // collegamenti, cadute, secondi collegato
  int32_t  rssiSum; uint32_t rssiN; int8_t rssiMin;
  uint8_t  apMax;                                 // massimo di telefoni insieme sull'hotspot (solo il numero)
  uint32_t bleStart, bleConn;
  uint32_t boot[4]; uint32_t bootLastSec;         // pressioni: brevi, 2 s, 8 s, 20 s; ultima (secondi dall'avvio, 0 = mai)
  uint32_t ledSec[4];                             // secondi per modo LED
};
static Stats g;
static uint32_t g_last = 0;
static bool g_wasOk = false, g_wasBle = false, g_init = false;

static uint32_t upSec() { return millis() / 1000; }

void statsReset() { memset(&g, 0, sizeof(g)); g.rssiMin = 0; g.t0 = upSec(); g_wasOk = false; g_wasBle = false; }

void statNote(StatEvent e) {
  if (!cfg.statOn) return;
  switch (e) {
    case ST_BLE_START: g.bleStart++; break;
    case ST_BLE_CONN: g.bleConn++; break;
    case ST_BOOT_SHORT: g.boot[0]++; g.bootLastSec = upSec() ? upSec() : 1; break;
    case ST_BOOT_2S: g.boot[1]++; g.bootLastSec = upSec() ? upSec() : 1; break;
    case ST_BOOT_8S: g.boot[2]++; g.bootLastSec = upSec() ? upSec() : 1; break;
    case ST_BOOT_20S: g.boot[3]++; g.bootLastSec = upSec() ? upSec() : 1; break;
  }
}

void statsTick() {
  if (!g_init) { statsReset(); g_init = true; }
  if (!cfg.statOn) return;
  uint32_t now = millis();
  if (now - g_last < 10000) return;
  g_last = now;
  bool ok = netState() == NET_CLIENT_OK;
  if (ok && !g_wasOk) g.wifiUp++;
  if (!ok && g_wasOk) g.wifiDown++;
  g_wasOk = ok;
  if (ok) {
    g.wifiSec += 10;
    int r = drvWifiRssi();
    if (r < 0 && r > -127) { g.rssiSum += r; g.rssiN++; if (g.rssiMin == 0 || r < g.rssiMin) g.rssiMin = (int8_t)r; }
  }
  if (netState() == NET_AP) { uint8_t n = drvWifiApClients(); if (n > g.apMax) g.apMax = n; }
  bool ble = bleRunning();
  if (ble && !g_wasBle) g.bleStart++;
  g_wasBle = ble;
  if (cfg.ledMode < 4) g.ledSec[cfg.ledMode] += 10;
}

static String vals(bool csv) {
  String s;
  int avg = g.rssiN ? (int)(g.rssiSum / (int32_t)g.rssiN) : 0;
  if (csv) {
    s += "name,value\n";
    s += "seconds_counting," + String((unsigned long)(upSec() - g.t0)) + "\n";
    s += "wifi_connects," + String((unsigned long)g.wifiUp) + "\nwifi_drops," + String((unsigned long)g.wifiDown) + "\nwifi_seconds_connected," + String((unsigned long)g.wifiSec) + "\n";
    s += "rssi_avg_dbm," + String(avg) + "\nrssi_min_dbm," + String((int)g.rssiMin) + "\nhotspot_max_phones," + String((int)g.apMax) + "\n";
    s += "ble_starts," + String((unsigned long)g.bleStart) + "\nble_connections," + String((unsigned long)g.bleConn) + "\n";
    s += "boot_short," + String((unsigned long)g.boot[0]) + "\nboot_2s," + String((unsigned long)g.boot[1]) + "\nboot_8s," + String((unsigned long)g.boot[2]) + "\nboot_20s," + String((unsigned long)g.boot[3]) + "\n";
    s += "led_state_s," + String((unsigned long)g.ledSec[0]) + "\nled_heartbeat_s," + String((unsigned long)g.ledSec[1]) + "\nled_fixed_s," + String((unsigned long)g.ledSec[2]) + "\nled_off_s," + String((unsigned long)g.ledSec[3]) + "\n";
  }
  return s;
}

String statsCsv() { return vals(true); }

String statsJson() {
  int avg = g.rssiN ? (int)(g.rssiSum / (int32_t)g.rssiN) : 0;
  String j = "{\"on\":" + String(cfg.statOn ? "true" : "false") + ",\"sec\":" + String((unsigned long)(upSec() - g.t0));
  j += ",\"wifiUp\":" + String((unsigned long)g.wifiUp) + ",\"wifiDown\":" + String((unsigned long)g.wifiDown) + ",\"wifiSec\":" + String((unsigned long)g.wifiSec);
  j += ",\"rssiAvg\":" + String(avg) + ",\"rssiMin\":" + String((int)g.rssiMin) + ",\"apMax\":" + String((int)g.apMax);
  j += ",\"bleStart\":" + String((unsigned long)g.bleStart) + ",\"bleConn\":" + String((unsigned long)g.bleConn);
  j += ",\"boot\":[" + String((unsigned long)g.boot[0]) + "," + String((unsigned long)g.boot[1]) + "," + String((unsigned long)g.boot[2]) + "," + String((unsigned long)g.boot[3]) + "],\"bootLast\":" + String((unsigned long)(g.bootLastSec ? upSec() - g.bootLastSec : 0)) + ",\"bootEver\":" + String(g.bootLastSec ? "true" : "false");
  j += ",\"led\":[" + String((unsigned long)g.ledSec[0]) + "," + String((unsigned long)g.ledSec[1]) + "," + String((unsigned long)g.ledSec[2]) + "," + String((unsigned long)g.ledSec[3]) + "]}";
  return j;
}

String statsText() {
  if (!cfg.statOn) return String(tr("Statistiche spente (stats on per accenderle)")) + "\n";
  int avg = g.rssiN ? (int)(g.rssiSum / (int32_t)g.rssiN) : 0;
  String t = trf("Conteggio da %lu s", (unsigned long)(upSec() - g.t0)) + "\n";
  t += trf("Wi-Fi: collegamenti %lu, cadute %lu, collegato %lu s, segnale medio %d dBm, minimo %d dBm", (unsigned long)g.wifiUp, (unsigned long)g.wifiDown, (unsigned long)g.wifiSec, avg, (int)g.rssiMin) + "\n";
  t += trf("Hotspot: al massimo %d telefoni insieme", (int)g.apMax) + "\n";
  t += trf("Bluetooth: accensioni %lu, collegamenti %lu", (unsigned long)g.bleStart, (unsigned long)g.bleConn) + "\n";
  t += trf("Tasto BOOT: brevi %lu, 2 s %lu, 8 s %lu, 20 s %lu", (unsigned long)g.boot[0], (unsigned long)g.boot[1], (unsigned long)g.boot[2], (unsigned long)g.boot[3]) + "\n";
  t += trf("LED (secondi): stato %lu, battito %lu, fisso %lu, spento %lu", (unsigned long)g.ledSec[0], (unsigned long)g.ledSec[1], (unsigned long)g.ledSec[2], (unsigned long)g.ledSec[3]) + "\n";
  return t;
}
#endif  // VOS_WITH_STATS
