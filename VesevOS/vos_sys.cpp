// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_sys.cpp
#include "vos_sys.h"
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
#include "esp_system.h"
#include "esp_freertos_hooks.h"
#include <LittleFS.h>
#include <Preferences.h>

#define HIST 60

static volatile uint32_t g_idle[2] = {0, 0};
static uint32_t g_maxIdle[2] = {1, 1};
static volatile int g_cpu = 0;
static float g_temp = 0;
static uint8_t g_cpuH[HIST], g_cpuHn = 0;
static int16_t g_tempH[HIST];            // decimi di grado
static uint8_t g_tempHn = 0;
static uint32_t g_boot = 0;
static uint32_t g_lifeBase = 0;       // secondi di vita accumulati fino all'ultimo salvataggio (contaore, come un contachilometri)
static uint32_t g_lifeSaved = 0;      // uptime (s) al momento dell'ultimo salvataggio
static String g_reset;
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
static void monitorStart() { xTaskCreatePinnedToCore(monitorTask, "monitor", 3072, NULL, 1, NULL, 0); }

static void monitorTask(void*) {
  uint32_t last[2] = {0, 0};
  int lowCount = 0;
  bool skip = false;                 // salta la misura subito dopo un cambio di frequenza
  for (;;) {
    vTaskDelay(pdMS_TO_TICKS(1000));
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
      g_cpu = constrain((l0 + l1) / 2, 0, 100);
    }
    skip = false;
    g_temp = temperatureRead();
    { static uint32_t lifeTick = 0; if (++lifeTick >= 600) { lifeTick = 0; lifeSave(); } }   // ogni 10 minuti
    // allarme: sopra 80 C LED rosso lampeggiante, torna normale sotto 75 C
    if (!g_hot && g_temp > 80.0f) { g_hot = true; ledSetFault(true); vlog("ATTENZIONE: temperatura CPU alta (%.1f C)", g_temp); }
    else if (g_hot && g_temp < 75.0f) { g_hot = false; ledSetFault(false); vlog("Temperatura CPU tornata normale (%.1f C)", g_temp); }
    // scalatura automatica: sale subito se serve, scende piano se la CPU e libera
    {
      uint32_t target = mhz;
      if (cfg.cpuMhz != 0) { target = (g_temp > 80.0f) ? 80 : cfg.cpuMhz; lowCount = 0; }   // velocita fissa
      else if (g_temp > 80.0f) { target = 80; lowCount = 0; }
      else if (g_cpu >= 60) { target = (mhz <= 80) ? 160 : 240; lowCount = 0; }
      else if (g_cpu < 20) { if (++lowCount >= 5) { lowCount = 0; target = (mhz > 160) ? 160 : 80; } }
      else lowCount = 0;
      if (target != mhz) { setCpuFrequencyMhz(target); skip = true; }
    }
    if (g_cpuHn < HIST) { g_cpuH[g_cpuHn] = g_cpu; g_tempH[g_cpuHn] = (int16_t)(g_temp * 10); g_cpuHn++; g_tempHn = g_cpuHn; }
    else {
      memmove(g_cpuH, g_cpuH + 1, HIST - 1);
      memmove(g_tempH, g_tempH + 1, (HIST - 1) * sizeof(int16_t));
      g_cpuH[HIST - 1] = g_cpu; g_tempH[HIST - 1] = (int16_t)(g_temp * 10);
    }
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
  j += "\"cpu\":" + String(g_cpu) + ",";
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
static TaskDef g_defs[8]; static int g_ndefs = 0;
static const char* const OURS[] = {"led", "time", "monitor", "net", "rules", "mqtt"};

void sysTaskRegister(const char* name, SysTaskStart start, SysTaskStart stop) {
  for (int i = 0; i < g_ndefs; i++) if (strcmp(g_defs[i].n, name) == 0) { g_defs[i].f = start; g_defs[i].stop = stop; return; }
  if (g_ndefs < 8) { g_defs[g_ndefs].n = name; g_defs[g_ndefs].f = start; g_defs[g_ndefs].stop = stop; g_ndefs++; }
}
static int defOf(const char* name) {
  for (int i = 0; i < g_ndefs; i++) if (strcmp(g_defs[i].n, name) == 0) return i;
  return -1;
}
bool sysTaskRunning(const char* name) { return xTaskGetHandle(name) != NULL; }

static bool inList(const char* name, const char* const* list, int n) {
  for (int i = 0; i < n; i++) if (strcmp(name, list[i]) == 0) return true;
  return false;
}

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
    int type = inList(nm, OURS, 6) ? 1 : 0;
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
