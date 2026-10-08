// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_sys.cpp
#include "vos_ble.h"
#include "vos_traffic.h"
#include "vos_sys.h"
#include "vos_diario.h"
#include "vos_led.h"
#include <WiFi.h>
#include <esp_sleep.h>
#include "vos_i18n.h"
#include "vos_util.h"
#include "vos_log.h"
#include "vos_net.h"
#include "vos_mqtt.h"
#include "vos_led.h"
#include "vos_config.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"
#include "esp_system.h"
#include "esp_freertos_hooks.h"
#include <LittleFS.h>
#include <Preferences.h>
#include "vos_wd.h"
#include "vos_serial.h"
#include "vos_web.h"
#include "vos_audit.h"
#include "vos_fw.h"

#define HIST 60

static volatile uint32_t g_idle[2] = {0, 0};
static uint32_t g_maxIdle[2] = {1, 1};
static volatile int g_cpu = 0, g_cpu0 = 0, g_cpu1 = 0;
static float g_temp = 0;
static uint8_t g_cpuH[HIST], g_cpuHn = 0;
static uint8_t g_c0H[HIST], g_c1H[HIST], g_ramH[HIST], g_psH[HIST], g_flH[HIST];   // storie per i grafici della Home (percentuali)
static uint8_t g_flPct = 0;                // percentuale file in flash, riletta ogni 30 s (la lettura costa)
static int16_t g_tempH[HIST];            // decimi di grado
static uint8_t g_tempHn = 0;
static uint32_t g_boot = 0;
static uint32_t g_lifeBase = 0;       // secondi di vita accumulati fino all'ultimo salvataggio (contaore, come un contachilometri)
static uint32_t g_lifeSaved = 0;      // uptime (s) al momento dell'ultimo salvataggio
static String g_reset;
static char g_topName[16] = "";      // task piu attivo (non IDLE), aggiornato ogni secondo
static int  g_topPct = 0;
static bool g_hot = false;      // allarme temperatura

static bool idle0() { g_idle[0]++; return false; }
static bool idle1() { g_idle[1]++; return false; }

static String resetName(int r) {
  switch (r) {
    case 1:  return tr("Accensione");
    case 3:  return tr("Reset software");
    case 4:  return tr("Crash (panic)");
    case 5:  return tr("Interrupt watchdog");
    case 6:  return tr("Task watchdog");
    case 7:  return tr("Altro watchdog");
    case 8:  return tr("Uscita da deep sleep");
    case 9:  return tr("Brownout (poca tensione)");
    case 11: return tr("USB (apertura monitor seriale)");
    case 12: return tr("Reset da JTAG");
    default: return trf("Sconosciuto (%d)", (int)r);
  }
}

uint32_t sysLifeSec() { return g_lifeBase + (uint32_t)(esp_timer_get_time() / 1000000ULL) - g_lifeSaved; }

// Salva il contaore nella memoria NVS (separata dai file: non si azzera col ripristino di fabbrica)
static void lifeSave() {
  uint32_t up = (uint32_t)(esp_timer_get_time() / 1000000ULL);
  g_lifeBase += up - g_lifeSaved;
  g_lifeSaved = up;
  Preferences p;
  if (p.begin("vos", false)) { p.putUInt("life", g_lifeBase); p.end(); }
}

static void monitorTask(void*);
static void monitorStart() { xTaskCreatePinnedToCore(monitorTask, "monitor", 4608, NULL, 1, NULL, 0); }

// Carico vero per core dal tempo che i task IDLE0/IDLE1 NON hanno girato (stessa fonte del Task manager).
// La taratura a conteggio sbagliava dopo i cambi di frequenza (mostrava 100% con la scheda quasi ferma).
static bool idleRunTimeLoad(int& l0, int& l1) {
#if (configUSE_TRACE_FACILITY == 1) && (configGENERATE_RUN_TIME_STATS == 1)
  static uint32_t pTot = 0, pI0 = 0, pI1 = 0; static bool have = false;
  UBaseType_t n = uxTaskGetNumberOfTasks();
  TaskStatus_t* a = (TaskStatus_t*)malloc((n + 2) * sizeof(TaskStatus_t));
  if (!a) return false;
  uint32_t tot = 0;
  n = uxTaskGetSystemState(a, n + 2, &tot);
  uint32_t i0 = 0, i1 = 0; bool f0 = false, f1 = false;
  static uint32_t pnum[40], prt[40]; static int pcnt = 0;           // tempo di esecuzione precedente per numero di task
  static uint32_t nnum[40], nrt[40]; int ncnt = 0, bestPct = 0; const char* best = nullptr;
  uint32_t dtAll = tot > pTot ? tot - pTot : 0;
  for (UBaseType_t i = 0; i < n; i++) {
    if (strcmp(a[i].pcTaskName, "IDLE0") == 0) { i0 = a[i].ulRunTimeCounter; f0 = true; }
    else if (strcmp(a[i].pcTaskName, "IDLE1") == 0) { i1 = a[i].ulRunTimeCounter; f1 = true; }
    else if (ncnt < 40) {
      uint32_t id = a[i].xTaskNumber, rt = a[i].ulRunTimeCounter, old = rt;
      for (int k = 0; k < pcnt; k++) if (pnum[k] == id) { old = prt[k]; break; }
      nnum[ncnt] = id; nrt[ncnt] = rt; ncnt++;
      if (dtAll) { int pc = (int)(100ULL * (rt - old) / dtAll); if (pc > bestPct) { bestPct = pc; best = a[i].pcTaskName; } }
    }
  }
  if (best) { strncpy(g_topName, best, sizeof(g_topName) - 1); g_topName[sizeof(g_topName) - 1] = 0; g_topPct = bestPct; }
  else { g_topName[0] = 0; g_topPct = 0; }
  memcpy(pnum, nnum, ncnt * sizeof(uint32_t)); memcpy(prt, nrt, ncnt * sizeof(uint32_t)); pcnt = ncnt;
  free(a);
  bool ok = false;
  if (have && f0 && f1 && tot > pTot) {
    uint32_t dt = tot - pTot;
    int a0 = (int)(100ULL * (i0 - pI0) / dt), a1 = (int)(100ULL * (i1 - pI1) / dt);
    l0 = constrain(100 - a0, 0, 100); l1 = constrain(100 - a1, 0, 100); ok = true;
  }
  pTot = tot; pI0 = i0; pI1 = i1; have = f0 && f1;
  return ok;
#else
  (void)l0; (void)l1; return false;
#endif
}
static void monitorTask(void*) {
  uint32_t last[2] = {0, 0};
  int lowCount = 0;
  bool skip = false;                 // salta la misura subito dopo un cambio di frequenza
  wdWatch("monitor", 20);
  for (;;) {
    vTaskDelay(pdMS_TO_TICKS(1000));
    wdBeat("monitor");
    auditTick();
    uint32_t mhz = getCpuFrequencyMhz();
    uint32_t d0 = g_idle[0] - last[0], d1 = g_idle[1] - last[1];
    last[0] = g_idle[0]; last[1] = g_idle[1];
    if (!skip) {
      // conteggi riportati a 240 MHz, cosi la taratura vale per ogni frequenza
      uint32_t n0 = (uint32_t)((uint64_t)d0 * 240 / mhz), n1 = (uint32_t)((uint64_t)d1 * 240 / mhz);
      if (n0 > g_maxIdle[0]) g_maxIdle[0] = n0;
      if (n1 > g_maxIdle[1]) g_maxIdle[1] = n1;
      uint64_t e0 = (uint64_t)g_maxIdle[0] * mhz / 240, e1 = (uint64_t)g_maxIdle[1] * mhz / 240;
      if (e0 == 0) e0 = 1;
      if (e1 == 0) e1 = 1;
      int l0 = 100 - (int)(100ULL * d0 / e0);
      int l1 = 100 - (int)(100ULL * d1 / e1);
      { int m0, m1; if (idleRunTimeLoad(m0, m1)) { l0 = m0; l1 = m1; } }   // se ci sono le statistiche di FreeRTOS, valgono piu della taratura a conteggio
      g_cpu0 = constrain(l0, 0, 100); g_cpu1 = constrain(l1, 0, 100);
      g_cpu = constrain((l0 + l1) / 2, 0, 100);
    }
    skip = false;
    g_temp = temperatureRead();
    { static uint32_t ftick = 0; ftick++;                      // ogni 10 s: la RAM e a pezzi troppo piccoli? (avviso in pagina, spento con un po' di margine)
      if (ftick % 10 == 0 && ftick > 30) {
        uint32_t big = ESP.getMaxAllocHeap();
        if (bleRunning() || big > 40000) auditClear("frag");        // con il Bluetooth acceso il pezzo piccolo e normale (se ne va quando si spegne)
        else if (big < 30000) auditEvent(AUD_YELLOW, "frag", tr("Memoria frammentata: il pezzo di RAM libero piu grande e piccolo. Chiudi la pagina web o spegni i servizi che non servono: si libera da sola"));
        webTrimIdle();                                      // RAM a pezzi e nessuno usa la pagina: chiude i collegamenti HTTPS fermi
      } }
    { static uint32_t dtick = 0; dtick++;
      diaryTick(dtick, ESP.getMinFreeHeap(), g_topName, g_topPct, mhz);
      if (dtick <= 15 || dtick % 10 == 0) { char st[20]; wdStalest(st, sizeof(st)); diaryExtra(ESP.getMaxAllocHeap(), serStallCount(), st); }                       // istantanea per il diario dei riavvii (in RTC ogni 10 s)
      if (dtick % 60 == 0) vlogl(LG_DBG, "TOP: task piu attivo %s %d%%, CPU %d%% (core %d%% e %d%%), %u MHz, %d C, RAM libera %u KB, minima %u KB",
                                 g_topName[0] ? g_topName : "-", g_topPct, g_cpu, g_cpu0, g_cpu1, (unsigned)mhz, (int)g_temp, (unsigned)(ESP.getFreeHeap() / 1024), (unsigned)(ESP.getMinFreeHeap() / 1024)); }
    { static uint32_t lifeTick = 0; if (++lifeTick >= 600) { lifeTick = 0; lifeSave(); } }   // ogni 10 minuti
    // allarme: sopra 80 C LED rosso lampeggiante, torna normale sotto 75 C
    if (!g_hot && g_temp > 80.0f) { g_hot = true; ledSetFault(true); vlog("ATTENZIONE: temperatura CPU alta (%.1f C)", g_temp); }
    else if (g_hot && g_temp < 75.0f) { g_hot = false; ledSetFault(false); vlog("Temperatura CPU tornata normale (%.1f C)", g_temp); }
    // scalatura automatica: sale subito se serve, scende piano se la CPU e libera
    {
      uint32_t target = mhz;
      bool settle = millis() < 60000;      // primo minuto: niente cambi di frequenza (Wi-Fi, NTP e HTTPS si assestano; un cambio dopo un reset da USB ha dato un Task watchdog)
      if (settle && g_temp <= 80.0f) { lowCount = 0; }
      else if (cfg.cpuMhz != 0) { target = (g_temp > 80.0f) ? 80 : cfg.cpuMhz; lowCount = 0; }   // velocita fissa
      else if (g_temp > 80.0f) { target = 80; lowCount = 0; }
      else if (g_cpu >= 60) { target = (mhz <= 80) ? 160 : 240; lowCount = 0; }
      else if (g_cpu < 20) { if (++lowCount >= 5) { lowCount = 0; target = (mhz > 160) ? 160 : 80; } }
      else lowCount = 0;
      if (target != mhz) { setCpuFrequencyMhz(target); skip = true; }
    }
    { static uint32_t ftk = 0;
      if (ftk++ % 30 == 0) { size_t ft = LittleFS.totalBytes(); g_flPct = ft ? (uint8_t)constrain((int)(100ULL * LittleFS.usedBytes() / ft), 0, 100) : 0; }
    }
    trafficTick();
    uint32_t ht = ESP.getHeapSize(), pt = ESP.getPsramSize();
    uint8_t rp = ht ? (uint8_t)constrain((int)(100 - 100ULL * ESP.getFreeHeap() / ht), 0, 100) : 0;
    uint8_t pp = pt ? (uint8_t)constrain((int)(100 - 100ULL * ESP.getFreePsram() / pt), 0, 100) : 0;
    if (g_cpuHn >= HIST) {
      memmove(g_cpuH, g_cpuH + 1, HIST - 1); memmove(g_c0H, g_c0H + 1, HIST - 1); memmove(g_c1H, g_c1H + 1, HIST - 1);
      memmove(g_ramH, g_ramH + 1, HIST - 1); memmove(g_psH, g_psH + 1, HIST - 1); memmove(g_flH, g_flH + 1, HIST - 1);
      memmove(g_tempH, g_tempH + 1, (HIST - 1) * sizeof(int16_t));
      g_cpuHn = HIST - 1;
    }
    g_cpuH[g_cpuHn] = g_cpu; g_c0H[g_cpuHn] = g_cpu0; g_c1H[g_cpuHn] = g_cpu1;
    g_ramH[g_cpuHn] = rp; g_psH[g_cpuHn] = pp; g_flH[g_cpuHn] = g_flPct;
    g_tempH[g_cpuHn] = (int16_t)(g_temp * 10);
    g_cpuHn++; g_tempHn = g_cpuHn;
  }
}

void sysApplyCpuMode() {
  if (cfg.cpuMhz != 0) setCpuFrequencyMhz(cfg.cpuMhz);
}

void sysInit() {
  setCpuFrequencyMhz(cfg.cpuMhz ? cfg.cpuMhz : 160);   // se automatico parte da 160 e poi scala da solo
  Preferences p;
  p.begin("vos", false);
  g_boot = p.getUInt("boots", 0) + 1;
  p.putUInt("boots", g_boot);
  g_lifeBase = p.getUInt("life", 0);
  g_lifeSaved = 0;
  p.end();
  g_reset = resetName((int)esp_reset_reason());
  esp_register_freertos_idle_hook_for_cpu(idle0, 0);
  esp_register_freertos_idle_hook_for_cpu(idle1, 1);
  g_temp = temperatureRead();
  sysTaskRegister("monitor", monitorStart);
  monitorStart();
}

int sysCpuPercent() { return g_cpu; }
float sysCpuTemp() { return g_temp; }
uint64_t sysUptimeSec() { return (uint64_t)(esp_timer_get_time() / 1000000ULL); }
uint32_t sysBootCount() { return g_boot; }
String sysResetReason() { return g_reset; }
String sysResetName(int reason) { return resetName(reason); }
void sysTopTask(char* name, size_t n, int* pct) { strncpy(name, g_topName, n - 1); name[n - 1] = 0; if (pct) *pct = g_topPct; }

static String histJson(const uint8_t* a, int n) {
  String j = "[";
  for (int i = 0; i < n; i++) { if (i) j += ","; j += String(a[i]); }
  return j + "]";
}

String sysCpuHistoryJson() { return histJson(g_cpuH, g_cpuHn); }

String sysTempHistoryJson() {
  String j = "[";
  for (int i = 0; i < g_tempHn; i++) { if (i) j += ","; j += String(g_tempH[i] / 10.0f, 1); }
  return j + "]";
}

String sysStatusJson() {
  size_t flashT = LittleFS.totalBytes(), flashU = LittleFS.usedBytes();
  String j = "{";
  j += "\"name\":\"" VOS_NAME "\",\"version\":\"" VOS_VERSION "\",";
  j += "\"uptime\":\"" + uptimeStr(sysUptimeSec()) + "\",";
  j += "\"uptimeSec\":" + String((unsigned long)sysUptimeSec()) + ",";
  j += "\"boots\":" + String(g_boot) + ",";
  j += "\"lifeSec\":" + String((unsigned long)sysLifeSec()) + ",";
  j += "\"chip\":\"" + jsonEscape(String(ESP.getChipModel())) + "\",\"rev\":" + String((int)ESP.getChipRevision()) +
       ",\"cores\":" + String((int)ESP.getChipCores()) + ",\"flashChip\":" + String((unsigned long)ESP.getFlashChipSize()) +
       ",\"idf\":\"" + jsonEscape(String(ESP.getSdkVersion())) + "\",";
  j += "\"reset\":\"" + jsonEscape(g_reset) + "\",";
  j += "\"cpu\":" + String(g_cpu) + ",\"cpu0\":" + String(g_cpu0) + ",\"cpu1\":" + String(g_cpu1) + ",";
  j += "\"temp\":" + String(g_temp, 1) + ",";
  j += "\"tempUnit\":" + String(cfg.tempUnit) + ",";
  j += "\"hot\":" + String(g_hot ? "true" : "false") + ",";
  j += "\"cpuMhz\":" + String(getCpuFrequencyMhz()) + ",";
  j += "\"cpuMode\":" + String(cfg.cpuMhz) + ",";
  j += "\"heapFree\":" + String(ESP.getFreeHeap()) + ",";
  j += "\"heapTotal\":" + String(ESP.getHeapSize()) + ",";
  j += "\"psramFree\":" + String(ESP.getFreePsram()) + ",";
  j += "\"psramTotal\":" + String(ESP.getPsramSize()) + ",";
  j += "\"flashUsed\":" + String((unsigned long)flashU) + ",";
  j += "\"flashTotal\":" + String((unsigned long)flashT) + ",";
  j += "\"net\":" + netStatusJson() + ",";
  j += "\"mqtt\":" + String(mqttConnected() ? 2 : mqttRunning() ? 1 : 0) + ",";
  j += "\"cpuHist\":" + sysCpuHistoryJson() + ",";
  j += "\"cpu0Hist\":" + histJson(g_c0H, g_cpuHn) + ",\"cpu1Hist\":" + histJson(g_c1H, g_cpuHn) + ",";
  j += "\"ramHist\":" + histJson(g_ramH, g_cpuHn) + ",\"psHist\":" + histJson(g_psH, g_cpuHn) + ",\"flHist\":" + histJson(g_flH, g_cpuHn) + ",";
  j += trafficJson() + ",";
  j += "\"tempHist\":" + sysTempHistoryJson();
  j += "}";
  return j;
}

String sysTasksText() {
  String r;
  r += trf("Task attivi: %u", (unsigned)uxTaskGetNumberOfTasks()) + "\n";
#if (configUSE_TRACE_FACILITY == 1) && (configUSE_STATS_FORMATTING_FUNCTIONS == 1)
  static char buf[1024];
  vTaskList(buf);
  r += "Nome          Stato Prio Stack Num\n";
  r += cleanAscii(String(buf));
#else
  r += String(tr("(dettagli non disponibili in questa build)")) + "\n";
#endif
  return r;
}

// ---- Task per la pagina (JSON) e arresto dei task consentiti ----
// Tipo: 0 = sistema (protetto), 1 = VesevOS, 2 = app (futuro)
// Si possono fermare solo i task della lista qui sotto (lista consentita, non lista dei vietati).
// Lista consentita: i moduli registrano i loro task con sysTaskRegister (nome + funzione che lo avvia).
struct TaskDef { const char* n; SysTaskStart f; SysTaskStart stop; };
static TaskDef g_defs[12]; static int g_ndefs = 0;

void sysTaskRegister(const char* name, SysTaskStart start, SysTaskStart stop) {
  for (int i = 0; i < g_ndefs; i++) if (strcmp(g_defs[i].n, name) == 0) { g_defs[i].f = start; g_defs[i].stop = stop; return; }
  if (g_ndefs < 12) { g_defs[g_ndefs].n = name; g_defs[g_ndefs].f = start; g_defs[g_ndefs].stop = stop; g_ndefs++; }
}
static int defOf(const char* name) {
  for (int i = 0; i < g_ndefs; i++) if (strcmp(g_defs[i].n, name) == 0) return i;
  return -1;
}
bool sysTaskRunning(const char* name) { return xTaskGetHandle(name) != NULL; }

#if (configUSE_TRACE_FACILITY == 1)          // usati solo qui sotto: fuori da questo blocco darebbero l'avviso "non usata"
static const char* const OURS[] = {"led", "time", "monitor", "net", "rules", "mqtt", "mesh", "wd"};
static bool inList(const char* name, const char* const* list, int n) {
  for (int i = 0; i < n; i++) if (strcmp(name, list[i]) == 0) return true;
  return false;
}
#endif

String sysTasksJson() {
#if (configUSE_TRACE_FACILITY == 1)
  UBaseType_t n = uxTaskGetNumberOfTasks();
  TaskStatus_t* a = (TaskStatus_t*)malloc((n + 2) * sizeof(TaskStatus_t));
  if (!a) return "{\"ok\":false}";
  uint32_t total = 0;
  n = uxTaskGetSystemState(a, n + 2, &total);
  String j = "{\"ok\":true,\"total\":" + String((unsigned long)total) + ",\"tasks\":[";
  for (UBaseType_t i = 0; i < n; i++) {
    const char* nm = a[i].pcTaskName;
    int type = inList(nm, OURS, 8) ? 1 : 0;
    bool kill = defOf(nm) >= 0;
    if (i) j += ",";
    j += "{\"n\":\"" + jsonEscape(String(nm)) + "\",\"id\":" + String((unsigned)a[i].xTaskNumber) +
         ",\"s\":" + String((int)a[i].eCurrentState) + ",\"p\":" + String((unsigned)a[i].uxCurrentPriority) +
         ",\"stk\":" + String((unsigned long)a[i].usStackHighWaterMark) + ",\"rt\":" + String((unsigned long)a[i].ulRunTimeCounter) +
         ",\"t\":" + String(type) + ",\"k\":" + String(kill ? "true" : "false") + "}";
  }
  free(a);
  // task registrati ma fermati: compaiono con stato 9 (fermato) per poterli riavviare
  for (int d = 0; d < g_ndefs; d++) {
    if (xTaskGetHandle(g_defs[d].n)) continue;
    if (n) j += ",";
    n++;
    j += "{\"n\":\"" + String(g_defs[d].n) + "\",\"id\":0,\"s\":9,\"p\":0,\"stk\":0,\"rt\":0,\"t\":1,\"k\":true}";
  }
  return j + "]}";
#else
  return "{\"ok\":false}";
#endif
}

bool sysTaskKill(const String& name, String& err) {
  if (name.length() == 0 || name.length() > 16) { err = tr("Nome task non valido"); return false; }
  if (defOf(name.c_str()) < 0) { err = tr("Questo task e protetto: non si puo fermare"); return false; }
  TaskHandle_t h = xTaskGetHandle(name.c_str());
  if (!h) { err = tr("Task non trovato"); return false; }
  vlog("TASK: fermato '%s' su richiesta", name.c_str());
  int d = defOf(name.c_str());
  if (d >= 0 && g_defs[d].stop) g_defs[d].stop(); else vTaskDelete(h);
  return true;
}

bool sysTaskRestart(const String& name, String& err) {
  if (name.length() == 0 || name.length() > 16) { err = tr("Nome task non valido"); return false; }
  int d = defOf(name.c_str());
  if (d < 0) { err = tr("Questo task non si puo riavviare"); return false; }
  TaskHandle_t h = xTaskGetHandle(name.c_str());
  if (h) { if (g_defs[d].stop) g_defs[d].stop(); else vTaskDelete(h); vTaskDelay(pdMS_TO_TICKS(50)); }
  g_defs[d].f();
  vlog("TASK: %s '%s' su richiesta", h ? "riavviato" : "avviato", name.c_str());
  return true;
}

// Sonno profondo: consuma pochissimo. Nessuna sveglia programmata: si riaccende con il tasto RESET
// (il tasto BOOT no: tenuto premuto al risveglio porterebbe la scheda in modo programmazione).
void sysSleep() {
  vlog("SISTEMA: sonno profondo (si riaccende con RESET)");
  Serial.flush();
  ledShutdown();
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  delay(100);
  esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
  esp_deep_sleep_start();
}
