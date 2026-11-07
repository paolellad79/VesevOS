// VesevOS - prova sul computer del CONTROLLO "microSD sparita" (packages/vos_prog_where.cpp VERO, il resto finto). La lancia prova_sdpersa.sh.
// Prova: il gancio non crea il task se nessuna app usa la microSD; crea un solo task con stack di almeno 8 KB; non ne crea un secondo mentre il primo gira;
// il task ferma solo le app attive con i dati sulla microSD; l'elenco dei programmi sta sullo heap (il frame del task resta sotto 1 KB: con l'elenco sullo
// stack erano oltre 2 KB); finito il task si puo ripartire; "Ferma l'app prima di cambiare dove salva" usa lo stesso elenco.
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "vos_prog_where.h"
#include "vos_prog_data.h"
#include "vos_prog.h"
#include "vos_app.h"
#include "vos_audit.h"
#include "vos_eventbus.h"
#include "vos_i18n.h"
#include "vos_log.h"
#include "vos_sd.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "mini.h"

// ---- stato dei finti
static SdLostFn g_hook = nullptr;
static int g_appsOnSd = 0;                       // quello che dice progSdAppCount()
static int g_creates = 0, g_deletes = 0, g_stops = 0, g_logs = 0, g_audits = 0, g_events = 0;
static uint32_t g_lastStack = 0;
static const char* g_lastName = "";
static void (*g_taskFn)(void*) = nullptr;
static char* g_marker = nullptr;
static long g_deltaMax = 0;
static char g_stopped[4][26];
static ProgRun g_list[4];
static int g_listN = 0;

// ---- finti delle funzioni esterne
void sdSetLostHook(SdLostFn fn) { g_hook = fn; }
int progSdAppCount() { return g_appsOnSd; }
int progWhere(uint8_t kind, const char* name) { return name && strstr(name, "sd") ? PW_SD : PW_FLASH; }
bool progWhereStore(uint8_t, const char*, int, ErrBuf& err) { err = ""; return true; }
int appSavesData(const char*) { return 1; }
int progRunning(ProgRun* out, int max, bool) {
  if (g_marker) { long d = g_marker - (char*)__builtin_frame_address(0); if (d < 0) d = -d; if (d > g_deltaMax) g_deltaMax = d; }
  int n = 0;
  for (int i = 0; i < g_listN && n < max; i++) out[n++] = g_list[i];
  return n;
}
bool progStop(const char* name, ErrBuf& err) { err = ""; if (g_stops < 4) snprintf(g_stopped[g_stops], sizeof g_stopped[0], "%s", name); g_stops++; return true; }
void vlog(const char*, ...) { g_logs++; }
void auditEvent(AuditLevel, const char*, const char*) { g_audits++; }
void EventBus::publish(const char*, const char*, int32_t) { g_events++; }
const char* tr(const char* it) { return it; }
TrText trf(const char* it, ...) { TrText t; va_list ap; va_start(ap, it); vsnprintf(t.small_, sizeof t.small_, it, ap); va_end(ap); return t; }
// FreeRTOS: il file prova_sdpersa_fake.cpp sostituisce xTaskCreatePinnedToCore
extern "C++" {
void vTaskDelete(TaskHandle_t) { g_deletes++; }
UBaseType_t uxTaskGetStackHighWaterMark(TaskHandle_t) { return 5000; }
}
void fakeCreateRecord(void (*fn)(void*), const char* name, uint32_t stack) { g_creates++; g_taskFn = fn; g_lastName = name; g_lastStack = stack; }

static void run(ProgRun* a, uint8_t kind, const char* name, bool active) {
  memset(a, 0, sizeof *a); a->kind = kind; snprintf(a->name, sizeof a->name, "%s", name); a->active = active;
}

int main() {
  progDataStart();
  CHECK(g_hook != nullptr);

  g_appsOnSd = 0; g_hook();
  CHECK(g_creates == 0);                                   // nessuna app sulla microSD: nessun task

  g_appsOnSd = 1; g_hook();
  CHECK(g_creates == 1 && g_taskFn != nullptr);
  CHECK(strcmp(g_lastName, "appsd") == 0);
  CHECK(g_lastStack >= 8192);
  g_hook();
  CHECK(g_creates == 1);                                   // il primo task non e finito: non ne parte un secondo

  run(&g_list[0], PK_APP_B, "app-sd", true);               // sulla microSD, in corso: si ferma
  run(&g_list[1], PK_APP_E, "app-flash", true);            // sulla flash: resta
  run(&g_list[2], PK_APP_E, "vecchia-sd", false);          // sulla microSD ma gia finita: resta com'e
  run(&g_list[3], PK_SCRIPT, "script-sd", true);           // uno script non e un'app: non si tocca
  g_listN = 4;
  char marker; g_marker = &marker;
  g_taskFn(nullptr);
  g_marker = nullptr;
  CHECK(g_stops == 1 && strcmp(g_stopped[0], "app-sd") == 0);
  CHECK(g_audits == 1 && g_events == 1);
  CHECK(g_deletes == 1);
  CHECK(g_deltaMax < 1024);                                // il frame del task non contiene piu l'elenco (2112 byte)
  printf("  frame del task fino a progRunning: %ld byte\n", g_deltaMax);

  g_hook();
  CHECK(g_creates == 2);                                   // finito il task, si puo ripartire

  g_listN = 0; g_stops = 0;
  g_taskFn(nullptr);
  CHECK(g_stops == 0 && g_deletes == 2);                   // elenco vuoto: nessuna app fermata, il task si chiude lo stesso

  // progWhereSet: stesso elenco, ma con l'app in corso rifiuta
  ErrBuf e;
  run(&g_list[0], PK_APP_B, "app-sd", true); g_listN = 1;
  CHECK(!progWhereSet(PK_APP_B, "app-sd", PW_FLASH, e) && !e.empty());
  g_listN = 0;
  CHECK(progWhereSet(PK_APP_B, "app-sd", PW_FLASH, e));
  return done("prova_sdpersa");
}
