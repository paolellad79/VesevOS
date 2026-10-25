// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_time.cpp
#include <math.h>
#include "../drivers/vos_drv_task.h"
#include "vos_time.h"
#include "vos_eventbus.h"
#include "vos_sys.h"
#include "vos_i18n.h"
#include "vos_config.h"
#include "vos_util.h"
#include "vos_log.h"
#include "vos_audit.h"
#include "vos_hooks.h"
#include "../drivers/vos_drv_wifi.h"
#include <sys/time.h>
#include <time.h>
#include <esp_sntp.h>
#include "../drivers/vos_drv_time.h"
#include "../drivers/vos_drv_udp.h"

static volatile bool g_apply = true;
static bool g_ntpStarted = false;
static DrvUdp g_udp;                   // il task del server esiste solo se il server e acceso
static bool g_serving = false;

#define NTP_UNIX_OFFSET 2208988800UL   // secondi tra 1900 e 1970

// Avvia il client NTP (lwIP/esp_sntp): i nomi dei server li tiene lwIP per puntatore, quindi stanno in copie fisse.
// Il fuso (TZ) lo imposta la libc. Era configTzTime() di Arduino.
static void ntpStart(const char* tz, const char* s1, const char* s2) {
  static char srv[2][64];
  snprintf(srv[0], sizeof srv[0], "%s", s1);
  snprintf(srv[1], sizeof srv[1], "%s", s2 ? s2 : "");
  if (esp_sntp_enabled()) esp_sntp_stop();
  esp_sntp_setoperatingmode(ESP_SNTP_OPMODE_POLL);
  esp_sntp_setservername(0, srv[0]);
  if (srv[1][0]) esp_sntp_setservername(1, srv[1]);
  esp_sntp_init();
  setenv("TZ", tz, 1);
  tzset();
}

bool timeValid() { return time(nullptr) > 1700000000; }   // dopo novembre 2023

void timeApply() { g_apply = true; }

static volatile uint32_t g_lastSync = 0;        // epoch dell'ultima sincronizzazione riuscita
static uint32_t g_refEpoch = 0, g_refMs = 0;    // ora dell'ultima sincronizzazione accettata (0 = nessuna) e quando
static uint8_t  g_refused = 0;                  // quante volte di fila e stata scartata
static void onSync(struct timeval* tv) {
  static uint32_t lastLog = 0;                         // una riga ogni 24 ore: niente righe ogni ora nel registro
  uint32_t expected = g_refEpoch ? g_refEpoch + (drvMillis() - g_refMs) / 1000UL : 0;
  if (!utilTimeSyncOk((uint32_t)tv->tv_sec, expected, g_refused)) {         // l'NTP semplice non e autenticato: ora assurda = scartata
    g_refused++;
    if (expected) { struct timeval t2; t2.tv_sec = expected; t2.tv_usec = 0; settimeofday(&t2, NULL); }
    vlog("ATTENZIONE: ora ricevuta da NTP non credibile (%lu), scartata", (unsigned long)tv->tv_sec);
    auditEvent(AUD_YELLOW, "ntpbad", tr("Ora ricevuta da NTP non credibile: scartata. Controlla il server NTP e la rete"));
    return;
  }
  g_refused = 0; g_refEpoch = (uint32_t)tv->tv_sec; g_refMs = drvMillis(); auditClear("ntpbad");
  EventBus::publish("time.sync", "", 1);              // ora sincronizzata (accettata)
  uint32_t prev = g_lastSync; g_lastSync = (uint32_t)tv->tv_sec;
  if (!prev || g_lastSync - lastLog >= 86400UL || !lastLog) { vlog("TIME: ora sincronizzata con NTP"); lastLog = g_lastSync; }
}

bool timeEveryValid(long m) {
  static const long ok[] = {0, 15, 60, 360, 720, 1440, 10080};
  for (long v : ok) if (v == m) return true;
  return false;
}

// "AAAA-MM-GG HH:MM" o "AAAA-MM-GG HH:MM:SS" nell'ora locale della scheda
bool timeSetLocal(const char* s, ErrBuf& err) {
  // lettura a mano (niente sscanf: tirava dentro circa 16 KB di libreria): numeri separati da - - spazio : e : facoltativo
  long v[6] = {0, 0, 0, 0, 0, 0}; int n = 0;
  const char* p = s ? s : ""; const char SEP[] = "-- ::";
  while (n < 6) {
    while (*p == ' ' && n == 3) p++;
    char* e; long x = strtol(p, &e, 10);
    if (e == p) break;
    v[n++] = x; p = e;
    if (n == 6 || *p != SEP[n - 1]) break;
    p++;
  }
  int Y = (int)v[0], M = (int)v[1], D = (int)v[2], h = (int)v[3], mi = (int)v[4], se = (int)v[5];
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

void timeNowFmt(char* out, size_t n) {
  if (!out || !n) return;
  if (!timeValid()) { snprintf(out, n, "%s", tr("non impostata")); return; }
  time_t t = time(nullptr);
  struct tm tmv;
  localtime_r(&t, &tmv);
  static const char* dfmt[3] = { "%d/%m/%Y", "%Y-%m-%d", "%m/%d/%Y" };
  char d[16], h[16];
  strftime(d, sizeof(d), dfmt[cfg.dateFmt > 2 ? 0 : cfg.dateFmt], &tmv);
  strftime(h, sizeof(h), cfg.timeFmt ? "%I:%M:%S %p" : "%H:%M:%S", &tmv);
  snprintf(out, n, "%s %s", d, h);
}

void timeSetEpoch(uint32_t t) {
  struct timeval tv; tv.tv_sec = t; tv.tv_usec = 0;
  settimeofday(&tv, NULL);
  g_refEpoch = 0; g_refused = 0;                        // ora a mano: niente piu "ora attesa" da confrontare
  vlog("TIME: ora impostata a mano");
}

uint32_t timeLastSync() { return g_lastSync; }

// pezzo pubblico (pagina di accesso): solo ora, scarto dal UTC, fuso e formati
bool timePubJson(HeapBuf& j) {
  bool v = timeValid();
  time_t t = time(nullptr);
  struct tm tmv; localtime_r(&t, &tmv);
  struct tm gm; gmtime_r(&t, &gm); gm.tm_isdst = tmv.tm_isdst;
  long off = (long)((long long)t - (long long)mktime(&gm));   // scarto dal UTC in secondi (senza tm_gmtoff)
  return j.addf(",\"tv\":%s,\"te\":%lu,\"to\":%ld,\"tn\":\"", v ? "true" : "false", (unsigned long)t, off) &&
         j.addJson(cfg.tzName.c_str()) &&
         j.addf("\",\"df\":%d,\"tf\":%d", (int)cfg.dateFmt, (int)cfg.timeFmt);
}

bool timeJson(HeapBuf& j) {
  char now[32]; timeNowFmt(now, sizeof now);
  return j.addf("{\"valid\":%s,\"now\":\"%s\",\"epoch\":%lu,\"ntp\":%s,\"server\":\"", timeValid() ? "true" : "false", now, (unsigned long)time(nullptr), cfg.ntpOn ? "true" : "false") &&
         j.addJson(cfg.ntpServer.c_str()) && j.add("\",\"server2\":\"") && j.addJson(cfg.ntpServer2.c_str()) &&
         j.addf("\",\"serve\":%s,\"every\":%lu,\"last\":%lu,\"tz\":\"", cfg.ntpServe ? "true" : "false", (unsigned long)cfg.ntpEvery, (unsigned long)g_lastSync) &&
         j.addJson(cfg.tz.c_str()) &&
         j.addf("\",\"dateFmt\":%d,\"timeFmt\":%d,\"tempUnit\":%d,\"tzName\":\"", (int)cfg.dateFmt, (int)cfg.timeFmt, (int)cfg.tempUnit) &&
         j.addJson(cfg.tzName.c_str()) && j.add("\"}");
}

// ---- server NTP (SNTP semplice) ----
static void stopServer() {
  if (g_serving) { g_udp.end(); g_serving = false; }
}

static void put32(uint8_t* p, uint32_t v) { p[0] = v >> 24; p[1] = v >> 16; p[2] = v >> 8; p[3] = v; }

// Risposta a una richiesta NTP (48 byte). Se l'ora non e sicura non risponde.
static size_t ntpReply(const uint8_t* in, size_t n, uint8_t* out, size_t cap) {
  if (n < 48 || cap < 48 || !timeValid()) return 0;
  memset(out, 0, 48);
  uint8_t vn = (in[0] >> 3) & 7;
  if (vn < 1 || vn > 4) vn = 4;
  out[0] = (0 << 6) | (vn << 3) | 4;                  // LI=0, versione, modo server
  out[1] = 3;                                         // livello (stratum)
  out[2] = 6; out[3] = 0xEC;                          // intervallo e precisione
  memcpy(out + 12, "VOS", 3);
  struct timeval tv; gettimeofday(&tv, NULL);
  uint32_t sec = (uint32_t)tv.tv_sec + NTP_UNIX_OFFSET;
  uint32_t frac = (uint32_t)(((uint64_t)tv.tv_usec << 32) / 1000000ULL);
  put32(out + 16, sec); put32(out + 20, frac);        // riferimento
  memcpy(out + 24, in + 40, 8);                       // originale = trasmissione del client
  put32(out + 32, sec); put32(out + 36, frac);        // ricezione
  put32(out + 40, sec); put32(out + 44, frac);        // trasmissione
  return 48;
}

static void startServer() {
  if (g_serving) return;
  if (!g_udp.begin(123, ntpReply, "ntpsrv")) { vlog("TIME: porta 123 non disponibile"); return; }
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
    NetState ns = hookNetState();
    if (cfg.ntpOn && ns == NET_CLIENT_OK && !g_ntpStarted) {
      // frequenza: 0 = solo all'avvio (intervallo massimo, circa 49 giorni)
      sntp_set_sync_interval(cfg.ntpEvery ? cfg.ntpEvery * 60000UL : 0xFFFFFFF0UL);
      sntp_set_time_sync_notification_cb(onSync);
      ntpStart(cfg.tz.c_str(), cfg.ntpServer.c_str(), cfg.ntpServer2.length() ? cfg.ntpServer2.c_str() : nullptr);
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

static void timeStart() {
  drvTaskCreate(timeTask, "time", 4096, NULL, 1, NULL, DRV_CORE_ANY);   // core: automatico (prima: core 0 fisso)
}

void timeInit() {
  sysTaskRegister("time", timeStart);
  timeStart();
}

// Temperatura nell'unita scelta (C o F), arrotondata all'intero: i decimali sarebbero falsa precisione
void fmtTemp(float c, char* out, size_t n) {
  if (cfg.tempUnit) snprintf(out, n, "%ld F", lroundf(c * 9.0f / 5.0f + 32.0f));
  else snprintf(out, n, "%ld C", lroundf(c));
}
