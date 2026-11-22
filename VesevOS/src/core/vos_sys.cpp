// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_sys.cpp
#include "../boards/vos_board.h"
#include <math.h>
#include "../drivers/vos_drv_task.h"
#include "vos_hooks.h"
#include "vos_azioni.h"
#include "vos_sys.h"
#include "vos_diario.h"
#include "../drivers/vos_drv_wifi.h"
#include "../drivers/vos_drv_sonno.h"
#include "vos_i18n.h"
#include "vos_util.h"
#include "vos_log.h"
#include "vos_service.h"
#include "vos_config.h"
#include "vos_taratura.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"
#include "esp_system.h"
#include <freertos/FreeRTOS.h>
#include "esp_freertos_hooks.h"      // dopo FreeRTOS.h (altrimenti non compila)
#include "../drivers/vos_drv_fs.h"
#include "../drivers/vos_drv_nvs.h"
#include "vos_wd.h"
#include "vos_serial.h"
#include "vos_audit.h"
#include "vos_fw.h"
#include "../drivers/vos_drv_time.h"
#include "../drivers/vos_drv_console.h"
#include "../core/vos_clamp.h"
#include "../drivers/vos_drv_cpu.h"
#include "../drivers/vos_drv_heap.h"
#include "../drivers/vos_drv_sys.h"

#define HIST 60

static volatile uint32_t g_idle[DRV_CORES_MAX];     // conteggio del gancio di riposo, un contatore per core
static uint32_t g_maxIdle[DRV_CORES_MAX];
static volatile int g_cpu = 0;                      // media sui core presenti
static volatile int g_cpuCore[DRV_CORES_MAX];       // carico di ogni core presente
static float g_temp = 0;       // temperatura mostrata (con la taratura)
static float g_tempGrezza = 0; // temperatura vera del sensore: solo per la protezione termica (decisione dell'utente 20/11/2026)
static uint8_t g_cpuH[HIST], g_cpuHn = 0;
static uint8_t g_coreH[DRV_CORES_MAX][HIST], g_ramH[HIST], g_psH[HIST], g_flH[HIST];   // storie per i grafici della Home (percentuali)
static uint8_t g_flPct = 0;                // percentuale file in flash, riletta ogni 30 s (la lettura costa)
static int16_t g_tempH[HIST];            // gradi interi
static uint8_t g_tempHn = 0;
static uint32_t g_boot = 0;
static uint32_t g_lifeBase = 0;       // secondi di vita accumulati fino all'ultimo salvataggio (contaore, come un contachilometri)
static uint32_t g_lifeSaved = 0;      // uptime (s) al momento dell'ultimo salvataggio
static char g_reset[48] = "";
static char g_topName[16] = "";      // task piu attivo (non IDLE), aggiornato ogni secondo
static int  g_topPct = 0;
static bool g_hot = false;      // allarme temperatura

// Gancio di riposo: uno solo per tutti i core, conta sul core che lo esegue.
static bool idleHook() { int c = (int)xPortGetCoreID(); if (c < DRV_CORES_MAX) g_idle[c] = g_idle[c] + 1; return false; }

// Numero di core del chip, limitato a quanti ne sa gestire il programma.
static int coreCount() { int n = (int)drvChipCores(); return n < 1 ? 1 : n > DRV_CORES_MAX ? DRV_CORES_MAX : n; }

static void resetNameTo(int r, char* out, size_t n) {
  const char* t = nullptr;
  switch (r) {
    case 1:  t = tr("Accensione"); break;
    case 2:  t = tr("Reset da pin esterno"); break;
    case 3:  t = tr("Reset software"); break;
    case 4:  t = tr("Crash (panic)"); break;
    case 5:  t = tr("Interrupt watchdog"); break;
    case 6:  t = tr("Task watchdog"); break;
    case 7:  t = tr("Altro watchdog"); break;
    case 8:  t = tr("Uscita da deep sleep"); break;
    case 9:  t = tr("Brownout (poca tensione)"); break;
    case 10: t = tr("Reset da SDIO"); break;
    case 11: t = tr("USB (apertura monitor seriale)"); break;
    case 12: t = tr("Reset da JTAG"); break;
    case 13: t = tr("Errore eFuse"); break;
    case 14: t = tr("Disturbo di alimentazione (power glitch)"); break;
    case 15: t = tr("Blocco della CPU"); break;
    default: snprintf(out, n, tr("Sconosciuto (%d)"), r); return;
  }
  snprintf(out, n, "%s", t);
}

uint32_t sysLifeSec() { return g_lifeBase + (uint32_t)(esp_timer_get_time() / 1000000ULL) - g_lifeSaved; }

// Salva il contaore nella memoria NVS (separata dai file: non si azzera col ripristino di fabbrica)
static void lifeSave() {
  uint32_t up = (uint32_t)(esp_timer_get_time() / 1000000ULL);
  g_lifeBase += up - g_lifeSaved;
  g_lifeSaved = up;
  DrvNvs p;
  if (p.begin("vos", false)) { p.putUInt("life", g_lifeBase); p.end(); }
}
void sysLifeSave() { lifeSave(); }

static void monitorTask(void*);
static void monitorStart() {
  drvTaskCreate(monitorTask, "monitor", VOS_PILA_MONITOR, NULL, 1, NULL, DRV_CORE_ANY);   // core: automatico (prima: core 0 fisso)
}

// Carico vero per core dal tempo che i task IDLE0/IDLE1 NON hanno girato (stessa fonte del Task manager).
// La taratura a conteggio sbagliava dopo i cambi di frequenza (mostrava 100% con la scheda quasi ferma).
static bool idleRunTimeLoad(int* load, int nc) {
#if (configUSE_TRACE_FACILITY == 1) && (configGENERATE_RUN_TIME_STATS == 1)
  static uint32_t pTot = 0, pIdle[DRV_CORES_MAX]; static bool have = false;
  UBaseType_t n = uxTaskGetNumberOfTasks();
  TaskStatus_t* a = (TaskStatus_t*)malloc((n + 2) * sizeof(TaskStatus_t));
  if (!a) return false;
  uint32_t tot = 0;
  n = uxTaskGetSystemState(a, n + 2, &tot);
  uint32_t idle[DRV_CORES_MAX]; bool found[DRV_CORES_MAX];
  for (int c = 0; c < DRV_CORES_MAX; c++) { idle[c] = 0; found[c] = false; }
  static uint32_t pnum[40], prt[40]; static int pcnt = 0;           // tempo di esecuzione precedente per numero di task
  static uint32_t nnum[40], nrt[40]; int ncnt = 0, bestPct = 0; const char* best = nullptr;
  uint32_t dtAll = tot > pTot ? tot - pTot : 0;
  for (UBaseType_t i = 0; i < n; i++) {
    int core = -1;
    for (int c = 0; c < nc; c++) {
      char nm[8]; snprintf(nm, sizeof nm, "IDLE%d", c);
      if (strcmp(a[i].pcTaskName, nm) == 0) { core = c; break; }
    }
    if (core >= 0) { idle[core] = a[i].ulRunTimeCounter; found[core] = true; }
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
  bool all = true;
  for (int c = 0; c < nc; c++) all = all && found[c];
  bool ok = false;
  if (have && all && tot > pTot) {
    uint32_t dt = tot - pTot;
    for (int c = 0; c < nc; c++) load[c] = vosClamp(100 - (int)(100ULL * (idle[c] - pIdle[c]) / dt), 0, 100);
    ok = true;
  }
  pTot = tot; have = all;
  for (int c = 0; c < nc; c++) pIdle[c] = idle[c];
  return ok;
#else
  (void)load; (void)nc; return false;
#endif
}
static void monitorTask(void*) {
  uint32_t last[DRV_CORES_MAX];
  for (int c = 0; c < DRV_CORES_MAX; c++) last[c] = 0;
  const int nc = coreCount();
  wdWatch("monitor", wdLimit("monitor"));          // base 60 s
  for (;;) {
    vTaskDelay(pdMS_TO_TICKS(1000));
    wdBeat("monitor");
    auditTick();
    uint32_t mhz = drvCpuMhz();
    int load[DRV_CORES_MAX];
    for (int c = 0; c < nc; c++) {
      // conteggi riportati a 240 MHz. Con il gestore di energia la frequenza cambia anche dentro il secondo: e una stima (la misura buona e quella di FreeRTOS)
      uint32_t d = g_idle[c] - last[c];
      last[c] = g_idle[c];
      uint32_t n240 = (uint32_t)((uint64_t)d * 240 / mhz);
      if (n240 > g_maxIdle[c]) g_maxIdle[c] = n240;
      uint64_t e = (uint64_t)g_maxIdle[c] * mhz / 240;
      if (e == 0) e = 1;
      load[c] = 100 - (int)(100ULL * d / e);
    }
    { int m[DRV_CORES_MAX]; if (idleRunTimeLoad(m, nc)) for (int c = 0; c < nc; c++) load[c] = m[c]; }   // se ci sono le statistiche di FreeRTOS, valgono piu della taratura a conteggio
    int sum = 0;
    for (int c = 0; c < nc; c++) { g_cpuCore[c] = vosClamp(load[c], 0, 100); sum += g_cpuCore[c]; }
    g_cpu = vosClamp(sum / nc, 0, 100);
    g_tempGrezza = drvTempC();
    g_temp = tempCorretta(g_tempGrezza, (int)cfg.tempTarSoc);
    { static uint32_t ftick = 0; ftick++;                      // ogni 10 s: la RAM e a pezzi troppo piccoli? (avviso in pagina, spento con un po' di margine)
      if (ftick % 10 == 0 && ftick > 30) {
        uint32_t big = drvHeapMaxBlock();
        if (serviceState("ble") == SVC_ON || big > 40000) auditClear("frag");        // con il Bluetooth acceso il pezzo piccolo e normale (se ne va quando si spegne)
        else if (big < 30000) auditEvent(AUD_YELLOW, "frag", tr("Memoria frammentata: il pezzo di RAM libero piu grande e piccolo. Chiudi la pagina web o spegni i servizi che non servono: si libera da sola"));
        hookCall(g_hooks.webTrimIdle);                                      // RAM a pezzi e nessuno usa la pagina: chiude i collegamenti HTTPS fermi
      } }
    { static uint32_t dtick = 0; dtick++;
      diaryTick(dtick, drvHeapMinFree(), g_topName, g_topPct, mhz);
      if (dtick <= 15 || dtick % 10 == 0) { char st[20]; wdStalest(st, sizeof(st)); diaryExtra(drvHeapMaxBlock(), serStallCount(), st); }                       // istantanea per il diario dei riavvii (in RTC ogni 10 s)
      if (dtick % 60 == 0) vlogl(LG_DBG, "TOP: task piu attivo %s %d%%, CPU %d%% (media dei core), %u MHz, %d C, RAM libera %u KB, minima %u KB",
                                 g_topName[0] ? g_topName : "-", g_topPct, g_cpu, (unsigned)mhz, (int)g_temp, (unsigned)(drvHeapFree() / 1024), (unsigned)(drvHeapMinFree() / 1024)); }
    { static uint32_t lifeTick = 0; if (++lifeTick >= 600) { lifeTick = 0; lifeSave(); } }   // ogni 10 minuti
    // caldo: livelli, riduzione del massimo della CPU e allarme (soglie e regole in vos_cpupm.cpp)
    g_hot = hookCpuTick(g_tempGrezza, sysCpuMaxCorePercent()) >= 2;   // governor + caldo (gancio verso vos_cpupm): il solo punto che cambia la frequenza; 2 = THERM_REDUCED o peggio
    { // Spazio usato della memoria interna. Il calcolo vero scorre tutto il file system: con tante voci dura piu di 5 s (Task watchdog), quindi lo fa
      // solo qui, di rado (mai nei primi 20 s) e a priorita da idle. Il controllo "monitor" aspetta di piu mentre dura. Gli altri leggono il valore ricordato.
      uint32_t nowS = drvMillis() / 1000;
      bool fresh = drvFsUsedDue(nowS);
      if (fresh) {
        uint16_t lim = wdLimit("monitor");
        wdWatch("monitor", lim > WD_FS_REFRESH_SEC ? lim : WD_FS_REFRESH_SEC);   // base 300 s: il calcolo scorre tutto il file system
        drvFsUsedRefresh(nowS);
        wdWatch("monitor", wdLimit("monitor"));                                  // riletto: puo essere cambiato nel frattempo
      }
      static uint32_t ftk = 0;
      if (fresh | (ftk++ % 30 == 0)) { size_t ft = drvFsTotal(); g_flPct = ft ? (uint8_t)vosClamp((int)(100ULL * drvFsUsed() / ft), 0, 100) : 0; }
    }
    hookCall(g_hooks.trafficTick);
    uint32_t ht = drvHeapTotal(), pt = drvPsramTotal();
    uint8_t rp = ht ? (uint8_t)vosClamp((int)(100 - 100ULL * drvHeapFree() / ht), 0, 100) : 0;
    uint8_t pp = pt ? (uint8_t)vosClamp((int)(100 - 100ULL * drvPsramFree() / pt), 0, 100) : 0;
    if (g_cpuHn >= HIST) {
      memmove(g_cpuH, g_cpuH + 1, HIST - 1); for (int c = 0; c < coreCount(); c++) memmove(g_coreH[c], g_coreH[c] + 1, HIST - 1);
      memmove(g_ramH, g_ramH + 1, HIST - 1); memmove(g_psH, g_psH + 1, HIST - 1); memmove(g_flH, g_flH + 1, HIST - 1);
      memmove(g_tempH, g_tempH + 1, (HIST - 1) * sizeof(int16_t));
      g_cpuHn = HIST - 1;
    }
    g_cpuH[g_cpuHn] = g_cpu; for (int c = 0; c < coreCount(); c++) g_coreH[c][g_cpuHn] = g_cpuCore[c];
    g_ramH[g_cpuHn] = rp; g_psH[g_cpuHn] = pp; g_flH[g_cpuHn] = g_flPct;
    g_tempH[g_cpuHn] = (int16_t)lroundf(g_temp);
    g_cpuHn++; g_tempHn = g_cpuHn;
  }
}

void sysApplyCpuMode() { hookCpuApplyMode(); }

void sysInit() {
  // la frequenza non si cambia qui: i primi 15 secondi restano a quella di avvio, poi ci pensa il monitor (cpuPmTick)
  DrvNvs p;
  if (!p.begin("vos", false)) vlog("ATTENZIONE: NVS: non si apre lo spazio \"vos\", contatori riavvii e vita non salvati");
  g_boot = p.getUInt("boots", 0) + 1;
  p.putUInt("boots", g_boot);
  g_lifeBase = p.getUInt("life", 0);
  g_lifeSaved = 0;
  p.end();
  resetNameTo((int)esp_reset_reason(), g_reset, sizeof(g_reset));
  for (int c = 0; c < coreCount(); c++) { g_maxIdle[c] = 1; esp_register_freertos_idle_hook_for_cpu(idleHook, c); }
  g_tempGrezza = drvTempC();
  g_temp = tempCorretta(g_tempGrezza, (int)cfg.tempTarSoc);
  sysTaskRegister("monitor", monitorStart);
  monitorStart();
}

int sysCpuPercent() { return g_cpu; }
int sysCpuCorePercent(int core) { return core >= 0 && core < coreCount() ? g_cpuCore[core] : 0; }
int sysCpuMaxCorePercent() {            // regola unica per il governor: vale il core piu carico
  int m = 0;
  for (int c = 0; c < coreCount(); c++) if (g_cpuCore[c] > m) m = g_cpuCore[c];
  return m;
}
float sysCpuTemp() { return g_temp; }
float sysCpuTempLetta() { return tempCorretta(drvTempC(), (int)cfg.tempTarSoc); }   // lettura nuova del sensore del chip, con la taratura
uint64_t sysUptimeSec() { return (uint64_t)(esp_timer_get_time() / 1000000ULL); }
uint32_t sysBootCount() { return g_boot; }
const char* sysResetText() { return g_reset; }
void sysResetNameTo(int reason, char* out, size_t n) { resetNameTo(reason, out, n); }
void sysTopTask(char* name, size_t n, int* pct) { strncpy(name, g_topName, n - 1); name[n - 1] = 0; if (pct) *pct = g_topPct; }

// Scrive un testo formattato (printf) nel HeapBuf. Ogni pezzo e corto (< 200 byte).
static bool addFmt(HeapBuf& o, const char* fmt, ...) __attribute__((format(printf, 2, 3)));
static bool addFmt(HeapBuf& o, const char* fmt, ...) {
  char t[200];
  va_list ap; va_start(ap, fmt);
  int n = vsnprintf(t, sizeof(t), fmt, ap);
  va_end(ap);
  if (n < 0) return true;
  return o.add(t, n < (int)sizeof(t) ? (size_t)n : sizeof(t) - 1);
}

static bool histJson(HeapBuf& j, const uint8_t* a, int n) {
  bool ok = j.add('[');
  for (int i = 0; i < n && ok; i++) ok = (i ? j.add(',') : true) && j.addUint(a[i]);
  return ok && j.add(']');
}

bool sysCpuHistoryJson(HeapBuf& j) { return histJson(j, g_cpuH, g_cpuHn); }

bool sysTempHistoryJson(HeapBuf& j) {
  bool ok = j.add('[');
  for (int i = 0; i < g_tempHn && ok; i++) ok = (i ? j.add(',') : true) && addFmt(j, "%d", (int)g_tempH[i]);
  return ok && j.add(']');
}


bool sysStatusJson(HeapBuf& j) {
  size_t flashT = drvFsTotal(), flashU = drvFsUsed();
  char up[64];
  uptimeTo(sysUptimeSec(), up, sizeof(up));
  int mqtt = serviceState("mqtt") == SVC_ON ? 2 : serviceState("mqtt") != SVC_OFF ? 1 : 0;
  const char* chip = drvChipModel();
  const char* idf = drvSdkVersion();
  bool ok = j.add("{\"name\":\"" VOS_NAME "\",\"version\":\"" VOS_VERSION "\",");
  ok = ok && addFmt(j, "\"apiVersion\":%d,", (int)VOS_API_VERSION);
  ok = ok && j.add("\"uptime\":\"") && j.addJson(up, strlen(up));
  ok = ok && addFmt(j, "\",\"uptimeSec\":%lu,\"boots\":%lu,\"lifeSec\":%lu,", (unsigned long)sysUptimeSec(), (unsigned long)g_boot, (unsigned long)sysLifeSec());
  ok = ok && j.add("\"chip\":\"") && j.addJson(chip, strlen(chip));
  ok = ok && addFmt(j, "\",\"rev\":%d,\"cores\":%d,\"flashChip\":%lu,", (int)drvChipRevision(), (int)drvChipCores(), (unsigned long)drvFlashSize());
  ok = ok && j.add("\"idf\":\"") && j.addJson(idf, strlen(idf));
  ok = ok && j.add("\",\"reset\":\"") && j.addJson(g_reset, strlen(g_reset)) && j.add("\",");
  ok = ok && addFmt(j, "\"cpu\":%d,\"cpuCore\":[", (int)g_cpu);
  for (int c = 0; c < coreCount() && ok; c++) ok = (c ? j.add(',') : true) && j.addUint((uint32_t)g_cpuCore[c]);
  ok = ok && j.add("],");
  ok = ok && addFmt(j, "\"temp\":%.1f,\"tempUnit\":%d,\"hot\":%s,", (double)g_temp, (int)cfg.tempUnit, g_hot ? "true" : "false");
  { char cp[320]; int cn = hookCpuStatus(cp, sizeof cp); ok = ok && cn >= 0 && j.add(cp, (size_t)cn); }   // campi cpuMhz...cpuChanges, scritti da vos_cpupm
  ok = ok && addFmt(j, "\"heapFree\":%lu,\"heapTotal\":%lu,", (unsigned long)drvHeapFree(), (unsigned long)drvHeapTotal());
  ok = ok && addFmt(j, "\"psramFree\":%lu,\"psramTotal\":%lu,", (unsigned long)drvPsramFree(), (unsigned long)drvPsramTotal());
  { DrvMemMap m; drvMemMap(m);                                    // mappa della RAM (-1 = non misurato -> null)
    auto nul = [](int32_t v, char* o, size_t n) { if (v < 0) snprintf(o, n, "null"); else snprintf(o, n, "%ld", (long)v); };
    char a[16], d[16], b[16], r[16]; nul(m.fixedIram, a, sizeof a); nul(m.fixedData, d, sizeof d); nul(m.fixedBss, b, sizeof b); nul(m.fixedRest, r, sizeof r);
    ok = ok && addFmt(j, "\"mem\":{\"sram\":%lu,\"fixed\":%lu,\"iram\":%s,\"data\":%s,\"bss\":%s,\"rest\":%s,\"heapUsed\":%lu,\"heapMin\":%lu,\"heapMaxBlock\":%lu,\"flash\":%lu},",
                      (unsigned long)m.sramNominal, (unsigned long)m.fixedTotal, a, d, b, r, (unsigned long)m.heapUsed, (unsigned long)m.heapMin, (unsigned long)m.heapMaxBlock, (unsigned long)m.flashSize); }
  ok = ok && addFmt(j, "\"flashUsed\":%lu,\"flashTotal\":%lu,\"mqtt\":%d,", (unsigned long)flashU, (unsigned long)flashT, mqtt);
  ok = ok && j.add("\"cpuHist\":") && sysCpuHistoryJson(j);
  ok = ok && j.add(",\"cpuCoreHist\":[");
  for (int c = 0; c < coreCount() && ok; c++) ok = (c ? j.add(',') : true) && histJson(j, g_coreH[c], g_cpuHn);
  ok = ok && j.add(']');
  ok = ok && j.add(",\"ramHist\":") && histJson(j, g_ramH, g_cpuHn) && j.add(",\"psHist\":") && histJson(j, g_psH, g_cpuHn);
  ok = ok && j.add(",\"flHist\":") && histJson(j, g_flH, g_cpuHn) && j.add(",\"tempHist\":") && sysTempHistoryJson(j);
  return ok && j.add('}');
}

// Sonno profondo: consuma pochissimo. Nessuna sveglia programmata: si riaccende con il tasto RESET
// (il tasto BOOT no: tenuto premuto al risveglio porterebbe la scheda in modo programmazione).
void sysSleep() {
  vlog("SISTEMA: sonno profondo (si riaccende con RESET)");
  azionePubblica(AZ_SONNO, "", 0);
  hookRecFlush(1500);                                        // dati del registratore in attesa: scritti prima di dormire
  drvConFlush();
  hookCall(g_hooks.ledShutdown);
  drvWifiShutdown();
  drvDelayMs(100);
  drvSonnoProfondo(0, false);
}
