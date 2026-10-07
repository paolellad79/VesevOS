// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_wd.cpp
#include "vos_wd.h"
#include "vos_config.h"
#include "vos_net.h"
#include "vos_sys.h"
#include "vos_log.h"
#include "vos_i18n.h"
#include "vos_util.h"
#include "vos_time.h"
#include "vos_audit.h"
#include <WiFi.h>
#include <Preferences.h>
#include <time.h>

#define WD_N 10
struct Watch { const char* name; uint16_t to; uint32_t last; uint8_t restarts; bool used; };
static Watch g_w[WD_N];
static String g_reason;            // motivo dell'ultimo riavvio automatico (letto all'avvio)
static uint32_t g_netLost = 0, g_ramLow = 0;
static bool g_stopAuto = false;    // antiloop: troppi riavvii, il watchdog non riavvia piu

static Watch* find(const char* n, bool create) {
  for (int i = 0; i < WD_N; i++) if (g_w[i].used && strcmp(g_w[i].name, n) == 0) return &g_w[i];
  if (!create) return nullptr;
  for (int i = 0; i < WD_N; i++) if (!g_w[i].used) { g_w[i].used = true; g_w[i].name = n; g_w[i].last = millis(); g_w[i].restarts = 0; return &g_w[i]; }
  return nullptr;
}

void wdWatch(const char* t, uint16_t to) { Watch* w = find(t, true); if (w) { w->to = to; w->last = millis(); } }
void wdUnwatch(const char* t) { Watch* w = find(t, false); if (w) w->used = false; }
void wdBeat(const char* t) { Watch* w = find(t, false); if (w) w->last = millis(); }
String wdLastReason() { return g_reason; }

// antiloop: ricorda gli orari (in secondi di vita) degli ultimi 3 riavvii automatici
void wdReboot(const String& why) {
  Preferences p;
  if (p.begin("vos", false)) {
    uint32_t life = sysLifeSec();
    uint32_t a = p.getUInt("wd1", 0), b = p.getUInt("wd2", 0);
    p.putUInt("wd3", b); p.putUInt("wd2", a); p.putUInt("wd1", life);
    p.putString("wdwhy", why);
    p.putUInt("wdn", p.getUInt("wdn", 0) + 1);
    p.end();
  }
  vlog("WATCHDOG: riavvio automatico (%s)", why.c_str());
  delay(300);
  ESP.restart();
}

static bool tooMany() {
  Preferences p; uint32_t c = 0;
  if (p.begin("vos", true)) { c = p.getUInt("wd3", 0); p.end(); }
  return c && sysLifeSec() - c < 3600UL;            // 3 riavvii nell'ultima ora di vita
}

static void act(const String& why) {
  if (g_stopAuto || tooMany()) {
    if (!g_stopAuto) { g_stopAuto = true; auditEvent(AUD_RED, "wdloop", tr("Watchdog: troppi riavvii automatici in un'ora, riavvio sospeso. Controlla il registro")); }
    return;
  }
  wdReboot(why);
}

static void wdTask(void*) {
  uint32_t tick = 0;
  for (;;) {
    vTaskDelay(pdMS_TO_TICKS(1000));
    tick++;
    uint32_t now = millis();
    // 1) servizi bloccati: prima si prova a riavviare il servizio, poi la scheda
    if (cfg.wdTask) {
      for (int i = 0; i < WD_N; i++) {
        Watch& w = g_w[i];
        if (!w.used || !w.to) continue;
        if (now - w.last > (uint32_t)w.to * 1000UL) {
          if (!sysTaskRunning(w.name)) { w.last = now; continue; }   // fermato apposta: non e un blocco
          String err;
          if (w.restarts < 2 && sysTaskRestart(w.name, err)) {
            w.restarts++; w.last = now;
            vlog("WATCHDOG: servizio '%s' bloccato, riavviato (%u)", w.name, w.restarts);
            auditEvent(AUD_YELLOW, String("wd_") + w.name, trf("Watchdog: il servizio '%s' si era bloccato ed e stato riavviato", w.name));
          } else act(trf("servizio '%s' bloccato", w.name));
        }
      }
    }
    // 2) rete assente (non in modo aereo, solo se il Wi-Fi di casa e configurato)
    if (cfg.wdNet && !cfg.airOn && cfg.staEnabled && cfg.staSsid.length()) {
      if (netState() == NET_CLIENT_OK) g_netLost = 0;
      else if (!g_netLost) g_netLost = now ? now : 1;
      else if (now - g_netLost > (uint32_t)cfg.wdNetMin * 60000UL) { g_netLost = 0; act(trf("rete assente da %u minuti", (unsigned)cfg.wdNetMin)); }
    } else g_netLost = 0;
    // 3) RAM bassa per 60 secondi
    if (cfg.wdRam) {
      if (ESP.getFreeHeap() < (uint32_t)cfg.wdRamKb * 1024UL) { if (!g_ramLow) g_ramLow = now ? now : 1; else if (now - g_ramLow > 60000UL) { g_ramLow = 0; act(trf("RAM libera sotto %u KB", (unsigned)cfg.wdRamKb)); } }
      else g_ramLow = 0;
    }
    // 4) dopo N giorni di accensione
    if (cfg.wdUpDays && sysUptimeSec() > (uint64_t)cfg.wdUpDays * 86400ULL) wdReboot(trf("acceso da %u giorni", (unsigned)cfg.wdUpDays));
    // 5) orario programmato (serve l'ora valida); non nei primi 2 minuti dopo un riavvio
    if (cfg.wdAt >= 0 && timeValid() && sysUptimeSec() > 120) {
      time_t t = time(NULL); struct tm lt; localtime_r(&t, &lt);
      int wd = (lt.tm_wday + 6) % 7;                         // 0 = lunedi
      if ((cfg.wdDays >> wd) & 1 && lt.tm_hour * 60 + lt.tm_min == cfg.wdAt && lt.tm_sec < 5) {
        char b[8]; snprintf(b, sizeof(b), "%02d:%02d", cfg.wdAt / 60, cfg.wdAt % 60);
        wdReboot(trf("riavvio programmato %s", b));
      }
    }
  }
}

static void wdStart() { xTaskCreatePinnedToCore(wdTask, "wd", 3072, NULL, 3, NULL, 0); }

void wdInit() {
  Preferences p;
  if (p.begin("vos", false)) { g_reason = p.getString("wdwhy", ""); p.remove("wdwhy"); p.end(); }
  if (g_reason.length()) { vlog("WATCHDOG: l'ultimo riavvio e stato automatico (%s)", g_reason.c_str()); auditEvent(AUD_YELLOW, "wdlast", trf("Ultimo riavvio automatico: %s", g_reason.c_str())); }
  // il ciclo principale (seriale, tasto BOOT) e protetto anche dal watchdog del chip
  enableLoopWDT();
  sysTaskRegister("wd", wdStart);
  wdStart();
}

String wdJson() {
  String j = "{\"task\":" + String(cfg.wdTask ? "true" : "false") + ",\"net\":" + String(cfg.wdNet ? "true" : "false") + ",\"ram\":" + String(cfg.wdRam ? "true" : "false") +
             ",\"netMin\":" + String(cfg.wdNetMin) + ",\"ramKb\":" + String(cfg.wdRamKb) + ",\"upDays\":" + String(cfg.wdUpDays) +
             ",\"at\":" + String(cfg.wdAt) + ",\"days\":" + String(cfg.wdDays) + ",\"stopped\":" + String(g_stopAuto ? "true" : "false") +
             ",\"last\":\"" + jsonEscape(g_reason) + "\",\"watch\":[";
  bool first = true;
  for (int i = 0; i < WD_N; i++) {
    if (!g_w[i].used) continue;
    if (!first) j += ","; first = false;
    j += "{\"n\":\"" + String(g_w[i].name) + "\",\"to\":" + String(g_w[i].to) + ",\"ago\":" + String((unsigned long)((millis() - g_w[i].last) / 1000)) + ",\"r\":" + String(g_w[i].restarts) + "}";
  }
  return j + "]}";
}

String wdText() {
  String t = trf("Watchdog: task %s, rete %s (%u min), RAM %s (%u KB)", cfg.wdTask ? "si" : "no", cfg.wdNet ? "si" : "no", (unsigned)cfg.wdNetMin, cfg.wdRam ? "si" : "no", (unsigned)cfg.wdRamKb) + "\n";
  for (int i = 0; i < WD_N; i++) if (g_w[i].used) t += trf("  %-8s ultimo segno di vita %lu s fa (limite %u s)", g_w[i].name, (unsigned long)((millis() - g_w[i].last) / 1000), g_w[i].to) + "\n";
  if (g_reason.length()) t += trf("Ultimo riavvio automatico: %s", g_reason.c_str()) + "\n";
  if (g_stopAuto) t += String(tr("Riavvii automatici sospesi (troppi in un'ora)")) + "\n";
  return t;
}
