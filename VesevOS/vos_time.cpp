// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_time.cpp
#include "vos_time.h"
#include "vos_sys.h"
#include "vos_i18n.h"
#include "vos_config.h"
#include "vos_net.h"
#include "vos_util.h"
#include "vos_log.h"
#include "vos_audit.h"
#include <WiFi.h>
#include <AsyncUDP.h>
#include <sys/time.h>
#include <time.h>
#include <esp_sntp.h>

static volatile bool g_apply = true;
static bool g_ntpStarted = false;
static AsyncUDP* g_udp = nullptr;      // creato solo se serve
static bool g_serving = false;

#define NTP_UNIX_OFFSET 2208988800UL   // secondi tra 1900 e 1970

bool timeValid() { return time(nullptr) > 1700000000; }   // dopo novembre 2023

void timeApply() { g_apply = true; }

static volatile uint32_t g_lastSync = 0;        // epoch dell'ultima sincronizzazione riuscita
static uint32_t g_refEpoch = 0, g_refMs = 0;    // ora dell'ultima sincronizzazione accettata (0 = nessuna) e quando
static uint8_t  g_refused = 0;                  // quante volte di fila e stata scartata
static void onSync(struct timeval* tv) {
  static uint32_t lastLog = 0;                         // una riga ogni 24 ore: niente righe ogni ora nel registro
  uint32_t expected = g_refEpoch ? g_refEpoch + (millis() - g_refMs) / 1000UL : 0;
  if (!utilTimeSyncOk((uint32_t)tv->tv_sec, expected, g_refused)) {         // l'NTP semplice non e autenticato: ora assurda = scartata
    g_refused++;
    if (expected) { struct timeval t2; t2.tv_sec = expected; t2.tv_usec = 0; settimeofday(&t2, NULL); }
    vlog("ATTENZIONE: ora ricevuta da NTP non credibile (%lu), scartata", (unsigned long)tv->tv_sec);
    auditEvent(AUD_YELLOW, "ntpbad", tr("Ora ricevuta da NTP non credibile: scartata. Controlla il server NTP e la rete"));
    return;
  }
  g_refused = 0; g_refEpoch = (uint32_t)tv->tv_sec; g_refMs = millis(); auditClear("ntpbad");
  uint32_t prev = g_lastSync; g_lastSync = (uint32_t)tv->tv_sec;
  if (!prev || g_lastSync - lastLog >= 86400UL || !lastLog) { vlog("TIME: ora sincronizzata con NTP"); lastLog = g_lastSync; }
}

bool timeEveryValid(long m) {
  static const long ok[] = {0, 15, 60, 360, 720, 1440, 10080};
  for (long v : ok) if (v == m) return true;
  return false;
}

// "AAAA-MM-GG HH:MM" o "AAAA-MM-GG HH:MM:SS" nell'ora locale della scheda
bool timeSetLocal(const String& s, String& err) {
  int Y, M, D, h, mi, se = 0;
  int n = sscanf(s.c_str(), "%d-%d-%d %d:%d:%d", &Y, &M, &D, &h, &mi, &se);
  if (n < 5 || Y < 2024 || Y > 2099 || M < 1 || M > 12 || D < 1 || D > 31 || h < 0 || h > 23 || mi < 0 || mi > 59 || se < 0 || se > 59) {
    err = tr("Data o ora non valida (AAAA-MM-GG HH:MM)"); return false;
  }
  setenv("TZ", cfg.tz.c_str(), 1); tzset();
  struct tm tmv = {}; tmv.tm_year = Y - 1900; tmv.tm_mon = M - 1; tmv.tm_mday = D; tmv.tm_hour = h; tmv.tm_min = mi; tmv.tm_sec = se; tmv.tm_isdst = -1;
  time_t t = mktime(&tmv);
  if (t < 1700000000) { err = tr("Data o ora non valida (AAAA-MM-GG HH:MM)"); return false; }
  timeSetEpoch((uint32_t)t);
  return true;
}

String timeNowStr() {
  if (!timeValid()) return tr("non impostata");
  time_t t = time(nullptr);
  struct tm tmv;
  localtime_r(&t, &tmv);
  static const char* dfmt[3] = { "%d/%m/%Y", "%Y-%m-%d", "%m/%d/%Y" };
  char d[16], h[16];
  strftime(d, sizeof(d), dfmt[cfg.dateFmt > 2 ? 0 : cfg.dateFmt], &tmv);
  strftime(h, sizeof(h), cfg.timeFmt ? "%I:%M:%S %p" : "%H:%M:%S", &tmv);
  return String(d) + " " + String(h);
}

void timeSetEpoch(uint32_t t) {
  struct timeval tv; tv.tv_sec = t; tv.tv_usec = 0;
  settimeofday(&tv, NULL);
  g_refEpoch = 0; g_refused = 0;                        // ora a mano: niente piu "ora attesa" da confrontare
  vlog("TIME: ora impostata a mano");
}

uint32_t timeLastSync() { return g_lastSync; }

// pezzo pubblico (pagina di accesso): solo ora, scarto dal UTC, fuso e formati
String timePubJson() {
  bool v = timeValid();
  time_t t = time(nullptr);
  struct tm tmv; localtime_r(&t, &tmv);
  struct tm gm; gmtime_r(&t, &gm); gm.tm_isdst = tmv.tm_isdst;
  long off = (long)((long long)t - (long long)mktime(&gm));   // scarto dal UTC in secondi (senza tm_gmtoff)
  String j = String(",\"tv\":") + (v ? "true" : "false");
  j += ",\"te\":" + String((unsigned long)t) + ",\"to\":" + String(off);
  j += ",\"tn\":\"" + jsonEscape(cfg.tzName) + "\",\"df\":" + String(cfg.dateFmt) + ",\"tf\":" + String(cfg.timeFmt);
  return j;
}

String timeJson() {
  String j = "{";
  j += "\"valid\":" + String(timeValid() ? "true" : "false") + ",";
  j += "\"now\":\"" + timeNowStr() + "\",";
  j += "\"epoch\":" + String((unsigned long)time(nullptr)) + ",";
  j += "\"ntp\":" + String(cfg.ntpOn ? "true" : "false") + ",";
  j += "\"server\":\"" + jsonEscape(cfg.ntpServer) + "\",\"server2\":\"" + jsonEscape(cfg.ntpServer2) + "\",";
  j += "\"serve\":" + String(cfg.ntpServe ? "true" : "false") + ",";
  j += "\"every\":" + String((unsigned long)cfg.ntpEvery) + ",\"last\":" + String((unsigned long)g_lastSync) + ",";
  j += "\"tz\":\"" + jsonEscape(cfg.tz) + "\",";
  j += "\"dateFmt\":" + String(cfg.dateFmt) + ",\"timeFmt\":" + String(cfg.timeFmt) + ",\"tempUnit\":" + String(cfg.tempUnit) + ",";
  j += "\"tzName\":\"" + jsonEscape(cfg.tzName) + "\"";
  j += "}";
  return j;
}

// ---- server NTP (SNTP semplice) ----
static void stopServer() {
  if (g_serving && g_udp) { g_udp->close(); g_serving = false; }
}

static void put32(uint8_t* p, uint32_t v) { p[0] = v >> 24; p[1] = v >> 16; p[2] = v >> 8; p[3] = v; }

static void startServer() {
  if (g_serving) return;
  if (!g_udp) g_udp = new AsyncUDP();
  if (!g_udp->listen(123)) { vlog("TIME: porta 123 non disponibile"); return; }
  g_udp->onPacket([](AsyncUDPPacket pk) {
    if (pk.length() < 48 || !timeValid()) return;      // se l'ora non e sicura non risponde
    uint8_t out[48];
    memset(out, 0, sizeof(out));
    uint8_t vn = (pk.data()[0] >> 3) & 7;
    if (vn < 1 || vn > 4) vn = 4;
    out[0] = (0 << 6) | (vn << 3) | 4;                  // LI=0, versione, modo server
    out[1] = 3;                                         // livello (stratum)
    out[2] = 6; out[3] = 0xEC;                          // intervallo e precisione
    memcpy(out + 12, "VOS", 3);
    struct timeval tv; gettimeofday(&tv, NULL);
    uint32_t sec = (uint32_t)tv.tv_sec + NTP_UNIX_OFFSET;
    uint32_t frac = (uint32_t)(((uint64_t)tv.tv_usec << 32) / 1000000ULL);
    put32(out + 16, sec); put32(out + 20, frac);        // riferimento
    memcpy(out + 24, pk.data() + 40, 8);                // originale = trasmissione del client
    put32(out + 32, sec); put32(out + 36, frac);        // ricezione
    put32(out + 40, sec); put32(out + 44, frac);        // trasmissione
    pk.write(out, sizeof(out));
  });
  g_serving = true;
  vlog("TIME: server NTP attivo (UDP 123)");
}

static void timeTask(void*) {
  setenv("TZ", cfg.tz.c_str(), 1);
  tzset();
  for (;;) {
    if (g_apply) {
      g_apply = false; g_ntpStarted = false;
      if (!cfg.ntpServe) stopServer();
    }
    NetState ns = netState();
    if (cfg.ntpOn && ns == NET_CLIENT_OK && !g_ntpStarted) {
      // frequenza: 0 = solo all'avvio (intervallo massimo, circa 49 giorni)
      sntp_set_sync_interval(cfg.ntpEvery ? cfg.ntpEvery * 60000UL : 0xFFFFFFF0UL);
      sntp_set_time_sync_notification_cb(onSync);
      configTzTime(cfg.tz.c_str(), cfg.ntpServer.c_str(), cfg.ntpServer2.length() ? cfg.ntpServer2.c_str() : nullptr);
      g_ntpStarted = true;
      vlog("TIME: NTP avviato (%s%s%s, %s)", cfg.ntpServer.c_str(), cfg.ntpServer2.length() ? ", " : "", cfg.ntpServer2.c_str(), cfg.tzName.c_str());
    } else if (!cfg.ntpOn && !g_ntpStarted) {
      setenv("TZ", cfg.tz.c_str(), 1); tzset();           // solo il fuso, ora a mano
      g_ntpStarted = true;
    }
    if (cfg.ntpServe && !g_serving && (ns == NET_CLIENT_OK || ns == NET_AP)) startServer();
    vTaskDelay(pdMS_TO_TICKS(3000));
  }
}

static void timeStart() { xTaskCreatePinnedToCore(timeTask, "time", 4096, NULL, 1, NULL, 0); }

void timeInit() {
  sysTaskRegister("time", timeStart);
  timeStart();
}

// Temperatura nell'unita scelta (C o F)
String fmtTemp(float c) {
  if (cfg.tempUnit) return String(c * 9.0f / 5.0f + 32.0f, 1) + " F";
  return String(c, 1) + " C";
}
