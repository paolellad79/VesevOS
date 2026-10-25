// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_wd.cpp
#include "vos_wd.h"
#include "../drivers/vos_drv_task.h"
#include "vos_wd_rule.h"
#include "vos_config.h"
#include "vos_sys.h"
#include "vos_log.h"
#include "vos_i18n.h"
#include "vos_util.h"
#include "vos_time.h"
#include "vos_audit.h"
#include "../drivers/vos_drv_wifi.h"
#include "../drivers/vos_drv_nvs.h"
#include "../drivers/vos_drv_sys.h"
#include "../drivers/vos_drv_boot.h"
#include "vos_diario.h"
#include "vos_strbuf.h"
#include "vos_hooks.h"
#include <time.h>
#include "../drivers/vos_drv_time.h"
#include "../drivers/vos_drv_heap.h"

#define WD_N 10
struct Watch { const char* name; uint16_t to; uint32_t last; uint8_t restarts; bool used; };
static Watch g_w[WD_N];
static char g_reason[64];          // motivo dell'ultimo riavvio automatico (letto all'avvio; si tronca se piu lungo)
static uint32_t g_netLost = 0, g_ramLow = 0;
static bool g_stopAuto = false;    // antiloop: troppi riavvii, il watchdog non riavvia piu
static bool g_degraded = false;    // modalita ridotta: servizi opzionali spenti, la scheda resta raggiungibile
static char g_degOff[48];          // cosa e stato spento ("mqtt", "mesh, ble"...; tre nomi brevi bastano)

// aggiunge "what" a un elenco separato da virgole (si tronca se non ci sta)
static void listAdd(char* list, size_t cap, const char* what) {
  size_t n = strlen(list);
  StrBuf b(list + n, cap - n);
  if (n) b.add(", ");
  b.add(what);
}

static Watch* find(const char* n, bool create) {
  for (int i = 0; i < WD_N; i++) if (g_w[i].used && strcmp(g_w[i].name, n) == 0) return &g_w[i];
  if (!create) return nullptr;
  for (int i = 0; i < WD_N; i++) if (!g_w[i].used) { g_w[i].used = true; g_w[i].name = n; g_w[i].last = drvMillis(); g_w[i].restarts = 0; return &g_w[i]; }
  return nullptr;
}

void wdWatch(const char* t, uint16_t to) { Watch* w = find(t, true); if (w) { w->to = to; w->last = drvMillis(); } }
// limite in vigore per un servizio; un nome sconosciuto non ha un limite proprio: 60 s
uint16_t wdLimit(const char* t) { int i = wdLimIndex(t); return i >= 0 ? cfg.wdLim[i] : 60; }
// Cambio dal vivo: lo cambia solo se il servizio sta usando il limite di prima (se e in un allungamento temporaneo, per esempio
// il monitor mentre conta lo spazio file, il limite nuovo arriva alla prossima registrazione).
void wdRetime(const char* t, uint16_t oldSec, uint16_t newSec) { Watch* w = find(t, false); if (w && w->to == oldSec) w->to = newSec; }
void wdUnwatch(const char* t) { Watch* w = find(t, false); if (w) w->used = false; }
void wdBeat(const char* t) { Watch* w = find(t, false); if (w) w->last = drvMillis(); }
const char* wdLastReason() { return g_reason; }
// il servizio controllato con il battito piu vecchio (per il diario dei riavvii): "nome 12s"; vuoto se nessuno
void wdStalest(char* out, size_t n) {
  if (!out || !n) return;
  out[0] = 0; uint32_t now = drvMillis(), best = 0; const char* nm = nullptr;
  for (int i = 0; i < WD_N; i++) if (g_w[i].used && g_w[i].to) { uint32_t a = now - g_w[i].last; if (!nm || a > best) { best = a; nm = g_w[i].name; } }
  if (nm) snprintf(out, n, "%s %lus", nm, (unsigned long)(best / 1000));
}

// antiloop: ricorda gli orari (in secondi di vita) degli ultimi 3 riavvii automatici
void wdReboot(const char* why) {
  sysLifeSave();                                    // contaore su NVS PRIMA di ricordare l'orario: dopo il riavvio riparte da qui e non indietro
  DrvNvs p;
  if (p.begin("vos", false)) {
    uint32_t life = sysLifeSec();
    uint32_t a = p.getUInt("wd1", 0), b = p.getUInt("wd2", 0);
    p.putUInt("wd3", b); p.putUInt("wd2", a); p.putUInt("wd1", life);
    p.putString("wdwhy", why);
    p.putUInt("wdn", p.getUInt("wdn", 0) + 1);
    p.end();
  }
  vlog("WATCHDOG: riavvio automatico (%s)", why);
  sysReboot(why, 300);
}

// ogni riavvio voluto passa da qui: il motivo resta nel diario dei riavvii (ultima riga dell'avvio che finisce)
void sysReboot(const char* why, uint32_t waitMs) {
  char b[64];
  snprintf(b, sizeof(b), "Riavvio: %s", why ? why : "?");
  diaryNote(b);
  if (waitMs) vTaskDelay(pdMS_TO_TICKS(waitMs));
  drvSysRestart();
}

static const char* g_laterWhy = "";
static uint32_t g_laterMs = 0;
static void rebootLaterTask(void*) { sysReboot(g_laterWhy, g_laterMs); }
void sysRebootLater(const char* why, uint32_t waitMs) {
  g_laterWhy = why ? why : ""; g_laterMs = waitMs;
  xTaskCreate(rebootLaterTask, "rst", 2048, NULL, 1, NULL);
}

static bool tooMany() {
  DrvNvs p; uint32_t c = 0;
  if (p.begin("vos", true)) { c = p.getUInt("wd3", 0); p.end(); }
  uint32_t now = sysLifeSec();
  static bool warned = false;                       // una sola riga di log per avvio
  if (wdFutureBad(now, c) && !warned) {
    warned = true;
    vlog("%s", trf("Watchdog: orario di riavvio ricordato nel futuro (+%lu s), ignorato", (unsigned long)wdFutureSec(now, c)).c_str());
  }
  return wdTooMany(now, c);                          // 3 riavvii nell'ultima ora di vita
}

// modalita ridotta (dopo troppi riavvii): non si riavvia piu. Se si sa quale servizio si e bloccato si spegne solo
// quello; se non si sa (rete, RAM) o e un servizio di base si spengono i servizi opzionali. Pagina web, log, OTA e
// Recovery restano. Vale fino al prossimo riavvio: niente viene cambiato nella configurazione salvata.
static void degrade(const char* why, const char* culprit) {
  char off[48] = "";
  char err[64];
  hookCall(g_hooks.ioAllOff);                                       // rele e altre uscite: stato sicuro (spente)
  bool opt = culprit && (!strcmp(culprit, "mqtt") || !strcmp(culprit, "mesh") || !strcmp(culprit, "led"));
  if (opt) {
    if (!strcmp(culprit, "led")) sysTaskKill("led", err, sizeof err); else hookDevOff(culprit);
    listAdd(off, sizeof(off), culprit);
  } else {
    const char* ids[] = {"mqtt", "mesh", "ble"};
    for (int i = 0; i < 3; i++) {
      if (hookDevIsOn(ids[i]) && hookDevOff(ids[i])) listAdd(off, sizeof(off), ids[i]);
    }
  }
  g_degraded = true;
  if (off[0]) listAdd(g_degOff, sizeof(g_degOff), off);
  vlog("WATCHDOG: modalita ridotta (%s), spenti: %s", why, off[0] ? off : "-");
  auditEvent(AUD_RED, "wdloop", trf("Watchdog: troppi riavvii automatici in un'ora (%s). Modalita ridotta: servizi spenti: %s. Pagina web, log e recupero restano attivi", why, off[0] ? off : "nessuno").c_str());
}

static void act(const char* why, const char* culprit = nullptr) {
  if (g_stopAuto || tooMany()) {
    if (!g_stopAuto) { g_stopAuto = true; degrade(why, culprit); }
    else if (culprit && (!strcmp(culprit, "mqtt") || !strcmp(culprit, "mesh") || !strcmp(culprit, "led")) && !strstr(g_degOff, culprit)) degrade(why, culprit);   // un altro servizio opzionale si blocca: spento anche lui
    return;
  }
  wdReboot(why);
}

static void wdTask(void*) {
  uint32_t tick = 0;
  for (;;) {
    vTaskDelay(pdMS_TO_TICKS(1000));
    tick++;
    uint32_t now = drvMillis();
    // 1) servizi bloccati: prima si prova a riavviare il servizio, poi la scheda
    if (cfg.wdTask) {
      for (int i = 0; i < WD_N; i++) {
        Watch& w = g_w[i];
        if (!w.used || !w.to) continue;
        if (now - w.last > (uint32_t)w.to * 1000UL) {
          if (!sysTaskRunning(w.name)) { w.last = now; continue; }   // fermato apposta: non e un blocco
          char err[64];
          if (w.restarts < 2 && sysTaskRestart(w.name, err, sizeof err)) {
            w.restarts++; w.last = now;
            vlog("WATCHDOG: servizio '%s' bloccato, riavviato (%u)", w.name, w.restarts);
            char akey[32]; snprintf(akey, sizeof akey, "wd_%s", w.name);
            auditEvent(AUD_YELLOW, akey, trf("Watchdog: il servizio '%s' si era bloccato ed e stato riavviato", w.name).c_str());
          } else { act(trf("servizio '%s' bloccato", w.name).c_str(), w.name); w.last = now; }   // se arriva qui non ha riavviato: non ripetere ogni secondo
        }
      }
    }
    // 2) rete assente (non in modo aereo, solo se il Wi-Fi di casa e configurato)
    if (cfg.wdNet && !cfg.airOn && cfg.staEnabled && cfg.staSsid.length()) {
      if (hookNetState() == NET_CLIENT_OK) g_netLost = 0;
      else if (!g_netLost) g_netLost = now ? now : 1;
      else if (now - g_netLost > (uint32_t)cfg.wdNetMin * 60000UL) { g_netLost = 0; act(trf("rete assente da %u minuti", (unsigned)cfg.wdNetMin).c_str()); }
    } else g_netLost = 0;
    // 3) RAM bassa per 60 secondi
    if (cfg.wdRam) {
      if (drvHeapFree() < (uint32_t)cfg.wdRamKb * 1024UL) { if (!g_ramLow) g_ramLow = now ? now : 1; else if (now - g_ramLow > 60000UL) { g_ramLow = 0; act(trf("RAM libera sotto %u KB", (unsigned)cfg.wdRamKb).c_str()); } }
      else g_ramLow = 0;
    }
    // 4) dopo N giorni di accensione
    if (cfg.wdUpDays && sysUptimeSec() > (uint64_t)cfg.wdUpDays * 86400ULL) wdReboot(trf("acceso da %u giorni", (unsigned)cfg.wdUpDays).c_str());
    // 5) orario programmato (serve l'ora valida); non nei primi 2 minuti dopo un riavvio
    if (cfg.wdAt >= 0 && timeValid() && sysUptimeSec() > 120) {
      time_t t = time(NULL); struct tm lt; localtime_r(&t, &lt);
      int wd = (lt.tm_wday + 6) % 7;                         // 0 = lunedi
      if ((cfg.wdDays >> wd) & 1 && lt.tm_hour * 60 + lt.tm_min == cfg.wdAt && lt.tm_sec < 5) {
        char b[12]; snprintf(b, sizeof(b), "%02d:%02d", cfg.wdAt / 60, cfg.wdAt % 60);
        wdReboot(trf("riavvio programmato %s", b).c_str());
      }
    }
  }
}

static void wdStart() {
  drvTaskCreate(wdTask, "wd", 6144, NULL, 3, NULL, DRV_CORE_ANY);   // core: automatico (prima: core 0 fisso)
}

void wdInit() {
  DrvNvs p;
  if (p.begin("vos", false)) { g_reason[0] = 0; p.getString("wdwhy", g_reason, sizeof(g_reason)); p.remove("wdwhy"); p.end(); }
  if (g_reason[0]) { vlog("WATCHDOG: l'ultimo riavvio e stato automatico (%s)", g_reason); auditEvent(AUD_YELLOW, "wdlast", trf("Ultimo riavvio automatico: %s", g_reason).c_str()); }
  // il ciclo principale (seriale, tasto BOOT) e protetto anche dal watchdog del chip
  drvLoopWdtEnable();
  sysTaskRegister("wd", wdStart);
  wdStart();
}

bool wdJson(HeapBuf& j) {
  bool ok = j.addf("{\"task\":%s,\"net\":%s,\"ram\":%s,\"netMin\":%d,\"ramKb\":%d,\"upDays\":%d,\"at\":%d,\"days\":%d,\"stopped\":%s,\"degraded\":%s,\"off\":\"",
                   cfg.wdTask ? "true" : "false", cfg.wdNet ? "true" : "false", cfg.wdRam ? "true" : "false",
                   (int)cfg.wdNetMin, (int)cfg.wdRamKb, (int)cfg.wdUpDays, (int)cfg.wdAt, (int)cfg.wdDays,
                   g_stopAuto ? "true" : "false", g_degraded ? "true" : "false") &&
            j.addJson(g_degOff) && j.add("\",\"last\":\"") && j.addJson(g_reason) && j.add("\",\"watch\":[");
  bool first = true;
  for (int i = 0; ok && i < WD_N; i++) {
    if (!g_w[i].used) continue;
    ok = (first || j.add(',')) &&
         j.addf("{\"n\":\"%s\",\"to\":%u,\"ago\":%lu,\"r\":%u}", g_w[i].name, (unsigned)g_w[i].to, (unsigned long)((drvMillis() - g_w[i].last) / 1000), (unsigned)g_w[i].restarts);
    first = false;
  }
  // tempi modificabili con i loro intervalli; INT_WDT solo da leggere (si cambia solo compilando)
  ok = ok && j.addf("],\"twdt\":{\"v\":%u,\"def\":%u,\"min\":%u,\"max\":%u},\"intMs\":%u,\"lim\":[",
                    (unsigned)drvTwdtSec(), (unsigned)WD_TWDT_DEF, (unsigned)WD_TWDT_MIN, (unsigned)WD_TWDT_MAX, (unsigned)drvIntWdtMs());
  for (int i = 0; ok && i < WD_LIM_N; i++) {
    ok = (i == 0 || j.add(',')) &&
         j.addf("{\"n\":\"%s\",\"v\":%u,\"def\":%u,\"min\":%u,\"max\":%u}", WD_LIMITS[i].name, (unsigned)cfg.wdLim[i],
                (unsigned)WD_LIMITS[i].def, (unsigned)WD_LIMITS[i].min, (unsigned)WD_LIMITS[i].max);
  }
  return ok && j.add("]}");
}

// una riga di intestazione, una per servizio controllato (max 10) e fino a 3 righe di avvisi: al massimo ~1,5 KB; 2048 bastano
std::string wdText() {
  StrText t(2048);
  StrBuf& s = t.buf();
  s.addf(tr("Watchdog: task %s, rete %s (%u min), RAM %s (%u KB)"), cfg.wdTask ? "si" : "no", cfg.wdNet ? "si" : "no", (unsigned)cfg.wdNetMin, cfg.wdRam ? "si" : "no", (unsigned)cfg.wdRamKb).add('\n');
  s.addf(tr("Task watchdog: %u s (da %u a %u), watchdog di interruzione: %u ms (solo compilazione)"), (unsigned)drvTwdtSec(), (unsigned)WD_TWDT_MIN, (unsigned)WD_TWDT_MAX, (unsigned)drvIntWdtMs()).add('\n');
  for (int i = 0; i < WD_N; i++) if (g_w[i].used) s.addf(tr("  %-8s ultimo segno di vita %lu s fa (limite %u s)"), g_w[i].name, (unsigned long)((drvMillis() - g_w[i].last) / 1000), g_w[i].to).add('\n');
  if (g_reason[0]) s.addf(tr("Ultimo riavvio automatico: %s"), g_reason).add('\n');
  if (g_stopAuto) s.add(tr("Riavvii automatici sospesi (troppi in un'ora)")).add('\n');
  if (g_degraded) s.addf(tr("Modalita ridotta: servizi spenti: %s"), g_degOff[0] ? g_degOff : "-").add('\n');
  return t.take();
}
