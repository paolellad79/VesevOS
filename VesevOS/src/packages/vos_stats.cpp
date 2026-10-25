// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_stats.cpp
#include "vos_stats.h"
#include "../core/vos_common.h"
#include "../core/vos_config.h"
#include "../net/vos_net.h"
#include "vos_ble.h"
#include "../core/vos_i18n.h"
#include "../core/vos_strbuf.h"
#include "../drivers/vos_drv_wifi.h"
#include "../drivers/vos_drv_time.h"

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

static uint32_t upSec() { return drvMillis() / 1000; }

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
  uint32_t now = drvMillis();
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
  g.ledSec[cfg.ledMode < 4 ? cfg.ledMode : (int)LED_STATE] += 10;   // flash, wifi e cpu mostrano lo stato: contano come "stato"
}

// CSV per il download: 18 righe, al massimo ~430 byte (i contatori sono uint32); 512 bastano, un solo blocco di heap
std::string statsCsv() {
  StrText t(512);
  StrBuf& s = t.buf();
  int avg = g.rssiN ? (int)(g.rssiSum / (int32_t)g.rssiN) : 0;
  s.add("name,value\n");
  s.addf("seconds_counting,%lu\n", (unsigned long)(upSec() - g.t0));
  s.addf("wifi_connects,%lu\nwifi_drops,%lu\nwifi_seconds_connected,%lu\n", (unsigned long)g.wifiUp, (unsigned long)g.wifiDown, (unsigned long)g.wifiSec);
  s.addf("rssi_avg_dbm,%d\nrssi_min_dbm,%d\nhotspot_max_phones,%d\n", avg, (int)g.rssiMin, (int)g.apMax);
  s.addf("ble_starts,%lu\nble_connections,%lu\n", (unsigned long)g.bleStart, (unsigned long)g.bleConn);
  s.addf("boot_short,%lu\nboot_2s,%lu\nboot_8s,%lu\nboot_20s,%lu\n", (unsigned long)g.boot[0], (unsigned long)g.boot[1], (unsigned long)g.boot[2], (unsigned long)g.boot[3]);
  s.addf("led_state_s,%lu\nled_heartbeat_s,%lu\nled_fixed_s,%lu\nled_off_s,%lu\n", (unsigned long)g.ledSec[0], (unsigned long)g.ledSec[1], (unsigned long)g.ledSec[2], (unsigned long)g.ledSec[3]);
  return t.take();
}

bool statsJson(HeapBuf& j) {
  int avg = g.rssiN ? (int)(g.rssiSum / (int32_t)g.rssiN) : 0;
  j.addf("{\"on\":%s,\"sec\":%lu", cfg.statOn ? "true" : "false", (unsigned long)(upSec() - g.t0));
  j.addf(",\"wifiUp\":%lu,\"wifiDown\":%lu,\"wifiSec\":%lu", (unsigned long)g.wifiUp, (unsigned long)g.wifiDown, (unsigned long)g.wifiSec);
  j.addf(",\"rssiAvg\":%d,\"rssiMin\":%d,\"apMax\":%d", avg, (int)g.rssiMin, (int)g.apMax);
  j.addf(",\"bleStart\":%lu,\"bleConn\":%lu", (unsigned long)g.bleStart, (unsigned long)g.bleConn);
  j.addf(",\"boot\":[%lu,%lu,%lu,%lu],\"bootLast\":%lu,\"bootEver\":%s", (unsigned long)g.boot[0], (unsigned long)g.boot[1], (unsigned long)g.boot[2], (unsigned long)g.boot[3],
         (unsigned long)(g.bootLastSec ? upSec() - g.bootLastSec : 0), g.bootLastSec ? "true" : "false");
  return j.addf(",\"led\":[%lu,%lu,%lu,%lu]}", (unsigned long)g.ledSec[0], (unsigned long)g.ledSec[1], (unsigned long)g.ledSec[2], (unsigned long)g.ledSec[3]);
}

// Testo per la shell: 6 righe tradotte, al massimo ~900 byte; 1024 bastano
std::string statsText() {
  StrText t(1024);
  StrBuf& s = t.buf();
  if (!cfg.statOn) { s.add(tr("Statistiche spente (stats on per accenderle)")).add('\n'); return t.take(); }
  int avg = g.rssiN ? (int)(g.rssiSum / (int32_t)g.rssiN) : 0;
  s.addf(tr("Conteggio da %lu s"), (unsigned long)(upSec() - g.t0)).add('\n');
  s.addf(tr("Wi-Fi: collegamenti %lu, cadute %lu, collegato %lu s, segnale medio %d dBm, minimo %d dBm"), (unsigned long)g.wifiUp, (unsigned long)g.wifiDown, (unsigned long)g.wifiSec, avg, (int)g.rssiMin).add('\n');
  s.addf(tr("Hotspot: al massimo %d telefoni insieme"), (int)g.apMax).add('\n');
  s.addf(tr("Bluetooth: accensioni %lu, collegamenti %lu"), (unsigned long)g.bleStart, (unsigned long)g.bleConn).add('\n');
  s.addf(tr("Tasto BOOT: brevi %lu, 2 s %lu, 8 s %lu, 20 s %lu"), (unsigned long)g.boot[0], (unsigned long)g.boot[1], (unsigned long)g.boot[2], (unsigned long)g.boot[3]).add('\n');
  s.addf(tr("LED (secondi): stato %lu, battito %lu, fisso %lu, spento %lu"), (unsigned long)g.ledSec[0], (unsigned long)g.ledSec[1], (unsigned long)g.ledSec[2], (unsigned long)g.ledSec[3]).add('\n');
  return t.take();
}
#endif  // VOS_WITH_STATS
