// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_cpupm.cpp
// Decide il tetto della CPU: la scelta dell'utente (velocita fissa o governor, vos_cpugov.*) abbassata se il chip e caldo,
// con il pavimento del tetto per HTTPS. Scala la frequenza il gestore di energia di ESP-IDF (drivers/vos_drv_cpu.*); qui si sceglie solo il tetto.
// Il monitor di sistema chiama cpuPmTick() una volta al secondo: e l'unico a chiamare il driver, cosi due task non si pestano i piedi.
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "vos_cpupm.h"
#include "vos_cpugov.h"
#include "vos_cpuboost.h"
#include "vos_config.h"
#include "vos_hooks.h"
#include "vos_audit.h"
#include "vos_i18n.h"
#include "vos_log.h"
#include "../drivers/vos_drv_cpu.h"
#include "../drivers/vos_drv_sys.h"
#include "../drivers/vos_drv_time.h"

// ---- Soglie di temperatura (lettura del sensore interno, gradi C) ----
// DATO ESPRESSIF: con l'intervallo (20,100) il sensore sbaglia meno di 2 C; fuori da 20..100 la lettura non e garantita.
// SCELTA NOSTRA (da tarare con le prove sulla scheda, vedi docs/progetto/piano-cpu-esp-pm-2026-10-22.md): i numeri qui sotto.
// Espressif non indica una soglia di riduzione ne una temperatura massima della giunzione.
static const float TEMP_WARN_C    = 75.0f;   // avviso (giallo se dura TEMP_WARN_SEC)
static const float TEMP_REDUCE_C  = 80.0f;   // riduzione della velocita
static const float TEMP_EXTREME_C = 90.0f;   // caldo estremo (10 C sotto il tetto del sensore)
static const float TEMP_BACK_C    = 70.0f;   // sotto questo valore torna tutto normale (isteresi 10 C = 5 volte l'errore del sensore)
static const uint32_t TEMP_WARN_SEC = 30;    // l'avviso compare dopo tanti secondi sopra TEMP_WARN_C
static const uint32_t TEMP_HOLD_SEC = 20;    // un livello termico non scende prima di tanti secondi dall'ultimo cambio
static const uint32_t SENSOR_BAD_SEC = 5;    // letture sbagliate di fila prima di dire "sensore in errore"
static const uint32_t HTTPS_FLOOR_MHZ = 160; // con HTTPS acceso il TETTO (massimo consentito) non scende sotto questo valore, finche il chip non e a caldo estremo. Il MINIMO resta sempre 80 (decisione utente 23/10: a riposo 80 anche con HTTPS): a 80 MHz la stretta di mano TLS blocca la CPU oltre 5 s (Task watchdog, analisi del 15/10)
static const uint32_t SAFE_MHZ = 160;        // tetto prudente con il sensore in errore
static const uint32_t BOOT_LOCK_MS = 15000;  // nei primi secondi dopo l'avvio nessun cambio di frequenza (resta quella di avvio)

static CpuThermal s_level = THERM_OK;
static uint32_t s_levelSince = 0;      // secondo (contatore interno) dell'ultimo cambio di livello
static uint32_t s_now = 0;             // contatore dei secondi
static uint32_t s_warnSec = 0;         // secondi di fila sopra TEMP_WARN_C
static uint32_t s_badSec = 0;          // letture sbagliate di fila
static bool     s_sensorBad = false;
static float    s_last[5];             // ultime temperature (mediana di 5: un picco isolato non conta)
static int      s_nLast = 0;
static uint32_t s_applied = 0;         // ultimo tetto chiesto al driver (0 = nessuno)
static bool     s_appliedFixed = false;
static bool     s_httpsOn = false;
static CpuGovState s_gov;              // stato del governor (solo il monitor lo tocca)
static CpuWhy   s_why = CW_START;
static uint32_t s_changes = 0;
static volatile bool s_force = false;  // la scelta e cambiata: riapplica anche se il tetto e lo stesso

// un gradino piu in basso: 240 -> 160 -> 80
static uint32_t stepDown(uint32_t mhz) { return mhz > 160 ? 160 : 80; }

// Tetto voluto adesso, in base a scelta, governor, caldo e HTTPS. fixed = true se va applicato come frequenza fissa.
static uint32_t wanted(bool& fixed, CpuWhy& why) {
  uint32_t choice = (uint32_t)cfg.cpuMhz;                 // 0 = decide il governor
  fixed = choice != 0;
  uint32_t cap = fixed ? choice : s_gov.cap;
  why = fixed ? CW_USER : s_gov.why == GW_BOOST ? CW_BOOST : CW_LOAD;
  if (choice != 80) {                                    // il 80 fisso scelto dall'utente non si tocca mai
    uint32_t t = cap;
    if (s_level == THERM_REDUCED) t = stepDown(cap);
    else if (s_level == THERM_EXTREME) t = fixed ? 80 : stepDown(stepDown(cap));
    if (t < cap) why = CW_HOT;
    // HTTPS: finche non e caldo estremo, il TETTO non scende sotto 160 (il minimo di esp_pm resta 80, vedi drvCpuAuto); se la scelta dell'utente era gia piu bassa, resta la sua
    if (s_httpsOn && s_level != THERM_EXTREME && t < HTTPS_FLOOR_MHZ) {
      t = cap < HTTPS_FLOOR_MHZ ? cap : HTTPS_FLOOR_MHZ;
      if (s_level >= THERM_REDUCED && t == HTTPS_FLOOR_MHZ) why = CW_HTTPS;
    }
    cap = t;
  }
  if (s_sensorBad && cap > SAFE_MHZ) { cap = SAFE_MHZ; why = CW_SENSOR; }   // senza sensore si va con prudenza
  return cap;
}

// Chiede il tetto al driver solo se e cambiato. Solo il monitor la chiama.
static void apply() {
  bool fixed = false;
  CpuWhy why = CW_LOAD;
  uint32_t cap = wanted(fixed, why);
  s_why = why;
  if (cap == s_applied && fixed == s_appliedFixed && !s_force) return;
  s_force = false;
  bool ok = fixed ? drvCpuFixed(cap) : drvCpuAuto(cap);
  if (!ok) {
    if (!drvCpuPmReady()) vlog("CPU: gestore di energia non presente in questa costruzione, frequenza non cambiata");
    else vlog("CPU: cambio non riuscito (%s %u MHz)", fixed ? "fisso" : "massimo", (unsigned)cap);
    return;
  }
  s_applied = cap; s_appliedFixed = fixed; s_changes++;
  vlog("CPU: %s %u MHz (%s, stato termico %d)", fixed ? "fisso" : "massimo", (unsigned)cap, cpuChoiceName(), (int)s_level);
}

void cpuPmApplyMode() { s_force = true; }                 // lo applica il monitor entro un secondo

uint32_t cpuPmLimitMhz() { uint32_t m = drvCpuMaxMhz(); return m ? m : drvCpuMhz(); }   // prima del primo cambio vale la frequenza di avvio
CpuThermal cpuPmThermal() { return s_level; }
CpuWhy cpuPmWhy() { return s_why; }

const char* cpuPmThermalText(CpuThermal t) {
  switch (t) {
    case THERM_OK:      return tr("normale");
    case THERM_WARN:    return tr("avviso (caldo)");
    case THERM_REDUCED: return tr("velocita ridotta per il caldo");
    case THERM_EXTREME: return tr("caldo estremo");
  }
  return tr("normale");
}

void cpuPmThresholds(float& warn, float& reduce, float& extreme, float& back) {
  warn = TEMP_WARN_C; reduce = TEMP_REDUCE_C; extreme = TEMP_EXTREME_C; back = TEMP_BACK_C;
}
uint32_t cpuPmChanges() { return s_changes; }

const char* cpuPmWhyKey(CpuWhy w) {
  switch (w) {
    case CW_START:  return "start";
    case CW_USER:   return "user";
    case CW_LOAD:   return "load";
    case CW_BOOST:  return "boost";
    case CW_HOT:    return "hot";
    case CW_HTTPS:  return "https";
    case CW_SENSOR: return "sensor";
  }
  return "load";
}

static float median5() {
  float v[5]; memcpy(v, s_last, sizeof(float) * s_nLast);
  for (int i = 1; i < s_nLast; i++) { float x = v[i]; int j = i - 1; while (j >= 0 && v[j] > x) { v[j + 1] = v[j]; j--; } v[j + 1] = x; }
  return v[s_nLast / 2];
}

static void thermalTick(float tempC) {
  s_now++;
  if (isnan(tempC)) {
    if (++s_badSec >= SENSOR_BAD_SEC && !s_sensorBad) {
      s_sensorBad = true;
      auditEvent(AUD_YELLOW, "tempsens", tr("Sensore di temperatura del chip non risponde: la velocita della CPU e limitata per prudenza"));
    }
  } else {
    s_badSec = 0;
    if (s_sensorBad) { s_sensorBad = false; auditClear("tempsens"); }
    if (s_nLast < 5) s_last[s_nLast++] = tempC;
    else { memmove(s_last, s_last + 1, sizeof(float) * 4); s_last[4] = tempC; }
    float t = median5();
    // livello che la temperatura chiede adesso (scendere vuole la soglia di ritorno, non quella di salita)
    CpuThermal want = s_level;
    if (t >= TEMP_EXTREME_C) want = THERM_EXTREME;
    else if (t >= TEMP_REDUCE_C && s_level < THERM_REDUCED) want = THERM_REDUCED;
    else if (t >= TEMP_WARN_C && s_level < THERM_WARN) want = THERM_WARN;
    else if (t < TEMP_BACK_C) want = THERM_OK;
    else if (s_level == THERM_EXTREME && t < TEMP_REDUCE_C) want = THERM_REDUCED;
    if (want > s_level || (want < s_level && s_now - s_levelSince >= TEMP_HOLD_SEC)) {   // sale subito, scende dopo una pausa
      CpuThermal old = s_level;
      s_level = want; s_levelSince = s_now;
      if (old < THERM_REDUCED && want >= THERM_REDUCED) { hookLedFault(true); vlog("ATTENZIONE: temperatura CPU alta (%ld C), velocita ridotta", lroundf(t)); }
      else if (old >= THERM_REDUCED && want < THERM_REDUCED) { hookLedFault(false); vlog("Temperatura CPU rientrata (%ld C)", lroundf(t)); }
    }
    s_warnSec = (t >= TEMP_WARN_C) ? s_warnSec + 1 : 0;
    if (s_warnSec == TEMP_WARN_SEC) auditEvent(AUD_YELLOW, "tempwarn", tr("Il chip e caldo da piu di 30 secondi: controlla l'aerazione"));
    else if (s_level == THERM_OK && s_warnSec == 0) auditClear("tempwarn");
    if (s_level == THERM_EXTREME) auditEvent(AUD_RED, "tempxtrm", tr("Caldo estremo: la CPU e al minimo possibile. Spegni i servizi che non servono, anche HTTPS se e acceso"));
    else auditClear("tempxtrm");
  }
  // HTTPS acceso o spento? Cambia il minimo ammesso. Avviso se la velocita fissa e troppo bassa per HTTPS.
  bool tls = hookWebHttpsUp();
  s_httpsOn = tls;
  static bool warned = false;
  bool low = tls && cfg.cpuMhz != 0 && cfg.cpuMhz < HTTPS_FLOOR_MHZ;
  if (low && !warned) { warned = true; vlog("ATTENZIONE: CPU fissa a %u MHz con HTTPS acceso", (unsigned)cfg.cpuMhz); auditEvent(AUD_YELLOW, "cpulow", tr("CPU fissa sotto 160 MHz con HTTPS acceso: la scheda puo bloccarsi e riavviarsi. Scegli un governor (ondemand, conservative, schedutil) o almeno 160 MHz")); }
  else if (!low && warned) { warned = false; auditClear("cpulow"); }
}

// Governor: un campione al secondo, dal momento in cui il blocco dell'avvio e finito.
static void governorTick(int busyPct) {
  if (cfg.cpuMhz != 0) { memset(&s_gov, 0, sizeof s_gov); return; }   // velocita fissa: il governor non decide (e riparte da zero se torna)
  CpuGovParams p = {cfg.cpuUp, cfg.cpuDown, cfg.cpuHold};
  CpuGovInput in;
  in.busyPct = (uint32_t)(busyPct < 0 ? 0 : busyPct > 100 ? 100 : busyPct);
  in.curMhz = drvCpuMaxMhz() ? drvCpuMaxMhz() : 240;     // finche non e stato chiesto nulla la scheda va a 240
  in.boost = cpuBoostActive();
  in.nowSec = s_now;
  cpuGovDecide((CpuGov)cfg.cpuGov, p, s_gov, in);
}

void cpuPmTick(float tempC, int busyPct) {
  thermalTick(tempC);
  if (drvMillis() < BOOT_LOCK_MS) { s_why = CW_START; return; }    // primi secondi: ne il governor ne il driver
  governorTick(busyPct);
  apply();
  if (s_why == CW_HTTPS) auditEvent(AUD_YELLOW, "cpuhttps", tr("Il chip e caldo: per HTTPS il tetto resta a 160 MHz (a riposo la CPU scende comunque a 80)"));
  else auditClear("cpuhttps");
}


// Campi dello stato della CPU per /api/status (li chiede vos_sys tramite il gancio cpuStatus).
int cpuPmStatusJson(char* out, size_t n) {
  int w = snprintf(out, n, "\"cpuMhz\":%lu,\"cpuMode\":%d,\"cpuMax\":%lu,\"cpuThermal\":%d,"
                           "\"cpuGov\":\"%s\",\"cpuWhy\":\"%s\",\"cpuUp\":%d,\"cpuDown\":%d,\"cpuHold\":%d,\"cpuChanges\":%lu,",
                   (unsigned long)drvCpuMhz(), (int)cfg.cpuMhz, (unsigned long)cpuPmLimitMhz(), (int)cpuPmThermal(),
                   cpuChoiceName(), cpuPmWhyKey(cpuPmWhy()), (int)cfg.cpuUp, (int)cfg.cpuDown, (int)cfg.cpuHold, (unsigned long)cpuPmChanges());
  return (w < 0 || (size_t)w >= n) ? -1 : w;
}
