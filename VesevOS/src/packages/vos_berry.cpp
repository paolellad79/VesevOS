// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_berry.cpp (vedi vos_berry.h). L'interprete e in src/third_party/berry (MIT, Guan Wenliang).
#include "vos_berry.h"
#include "../devices/vos_io.h"
#include "../core/vos_common.h"
#if VOS_WITH_BERRY
#include "../third_party/berry/src/berry.h"
#include <time.h>
#include <ctype.h>
#include "esp_heap_caps.h"
#include "vos_rules.h"
#include "vos_app_rt.h"
#include "../core/vos_log.h"
#include "../core/vos_time.h"
#include "../core/vos_sys.h"
#include "../core/vos_i18n.h"
#include "../core/vos_files.h"
#include "../core/vos_util.h"
#include "vos_berry_int.h"
#include "../devices/vos_led.h"
#include "vos_btn.h"
#include "vos_mqtt.h"

// ---------- stato dell'esecuzione (una sola alla volta) ----------
namespace vosberry {
portMUX_TYPE g_mux = portMUX_INITIALIZER_UNLOCKED;
volatile bool g_busy = false;       // un'esecuzione e in corso
static volatile bool g_done = false;       // il task ha finito (risultati pronti)
static volatile bool g_abandoned = false;  // il chiamante non aspetta piu: il task libera lui il "busy"
volatile bool g_toErr = false;             // dove va l'uscita: false = out, true = err
static char*    g_code = NULL;             // testo dello script (copia, liberato dal task dopo la lettura)
char     g_obuf[BERRY_OUT_MAX + 1]; size_t g_olen = 0; bool g_otrunc = false;
char     g_ebuf[BERRY_EBUF_SZ];     size_t g_elen = 0;
static char     g_msg[100];                // messaggio d'errore delle nostre funzioni (letto dopo la fine dei String)
static uint32_t g_deadline = 0;            // millis() oltre il quale lo script e fermato (comando "berry <codice>")
volatile bool g_bg = false;         // script di /scripts in sottofondo (senza scadenza, ma con le pause obbligatorie)
volatile bool g_stop = false;       // richiesta di fermare lo script
static uint32_t g_lastYield = 0;           // ultima pausa (sleep) dello script in sottofondo
char     g_name[25] = "";           // nome dello script in /scripts in corso o ultimo
static uint32_t g_beats = 0;
static const char* g_stackTop = NULL;      // cima dello stack del task (per misurare quanto ne usa lo script)
static bool     g_runOk = false;
volatile bool g_app = false;        // in corso c'e un'APP (file .vap): dopo il codice resta viva e riceve i comandi della pagina
static char     g_use[80] = "";           // permessi dell'app (use= del manifest)
// memoria
size_t   g_used = 0, g_peak = 0;
// statistiche
uint32_t g_runs = 0, g_lastMs = 0, g_lastPeak = 0, g_lastLeak = 0; bool g_lastOk = true;
}  // namespace vosberry
using namespace vosberry;

// ---------- limiti di tempo e di stack (ogni 64 istruzioni) ----------
// Berry chiama se stesso in C quando uno script usa funzioni come map/each con una funzione che richiama se stessa: ogni livello
// mangia stack del task. Per questo si misura lo stack usato e si ferma lo script prima che finisca (altrimenti la scheda si riavvia).
// Attenzione: be_raise salta (longjmp) fuori da qui: nessun oggetto C++ con distruttore in questa funzione.
static void obsHook(bvm* vm, int ev, ...) {
  if (ev != BE_OBS_VM_HEARTBEAT) return;
  char probe;
  if ((size_t)(g_stackTop - &probe) > BERRY_STACK_USE_MAX) be_raise(vm, "stack_error", "troppi livelli di chiamate (stack)");
  if (g_stop) be_raise(vm, "stopped_error", "script fermato");
  if (g_bg) { if ((int32_t)(millis() - g_lastYield) > (int32_t)BERRY_BURST_MS) be_raise(vm, "timeout_error", "script senza pause (usa sleep): fermato"); }
  else if ((int32_t)(millis() - g_deadline) > 0) be_raise(vm, "timeout_error", "script troppo lungo (limite di tempo)");
  if ((++g_beats & 255) == 0) vTaskDelay(1);               // cede il processore: gli altri task (e il controllo del sistema) non restano fermi
}

// ---------- coda degli script (azione "script <nome>" delle Automazioni) ----------
static char g_q[BERRY_QMAX][25]; static char g_qwho[BERRY_QMAX][16]; namespace vosberry { int g_qn = 0; }
static bool qPop(char* name, char* who) {
  bool ok = false;
  portENTER_CRITICAL(&g_mux);
  if (g_qn > 0) {
    strcpy(name, g_q[0]); strcpy(who, g_qwho[0]);
    for (int i = 1; i < g_qn; i++) { strcpy(g_q[i - 1], g_q[i]); strcpy(g_qwho[i - 1], g_qwho[i]); }
    g_qn--; ok = true;
  }
  portEXIT_CRITICAL(&g_mux);
  return ok;
}
void vosberry::qClear() { portENTER_CRITICAL(&g_mux); g_qn = 0; portEXIT_CRITICAL(&g_mux); }

// ---------- funzioni offerte allo script ----------
// Regola: nelle funzioni che possono dare errore NON restano String vive quando si chiama be_raise (longjmp senza distruttori).
static bool actDo(const char* pre, const char* arg) {
  // le azioni led* salvano la configurazione in flash: se uno script le ripete di continuo consuma la memoria. Tetto: 6 al minuto.
  static uint32_t cfgWin = 0; static int cfgN = 0;
  if (strncmp(arg, "led", 3) == 0 || strncmp(pre, "led", 3) == 0) {
    uint32_t now = millis();
    if (cfgN == 0 || now - cfgWin > 60000UL) { cfgWin = now; cfgN = 0; }
    if (++cfgN > 6) { strncpy(g_msg, "troppe modifiche salvate (max 6 al minuto): per cambiare il LED di continuo usa led(\"RRGGBB\")", sizeof(g_msg) - 1); g_msg[sizeof(g_msg) - 1] = 0; return false; }
  }
  const char* ap = arg; while (*ap == ' ' || *ap == '\t') ap++;
  if (strncmp(pre, "script", 6) == 0 || strncmp(ap, "script", 6) == 0 || strncmp(pre, "app", 3) == 0 || strncmp(ap, "app", 3) == 0) { strncpy(g_msg, "uno script non puo avviare altri script o app", sizeof(g_msg) - 1); g_msg[sizeof(g_msg) - 1] = 0; return false; }
  bool ok; String e;
  { String a = String(pre) + arg; ok = rulesAction(a, e, "BERRY"); }
  if (!ok) { strncpy(g_msg, e.c_str(), sizeof(g_msg) - 1); g_msg[sizeof(g_msg) - 1] = 0; }
  return ok;
}
// led("RRGGBB"): il LED prende quel colore SUBITO (qualunque modo sia scelto) e non si salva nulla; led("auto"): torna il modo normale.
// A fine script il LED torna normale da solo.
static int bfLed(bvm* vm) {
  if (be_top(vm) < 1 || !be_isstring(vm, 1)) be_raise(vm, "type_error", "led(\"RRGGBB\"): serve un testo di 6 cifre esadecimali (o \"auto\")");
  const char* c = be_tostring(vm, 1);
  if (strcmp(c, "auto") == 0) { ledScript(-1); be_return_nil(vm); }
  bool okc = strlen(c) == 6;
  for (int i = 0; okc && i < 6; i++) okc = isxdigit((unsigned char)c[i]) != 0;
  if (!okc) be_raise(vm, "value_error", "colore non valido: servono 6 cifre esadecimali, per esempio ff0000");
  ledScript((int32_t)strtoul(c, NULL, 16));
  be_return_nil(vm);
}
static int bfAct(bvm* vm) {                                 // act("gpio 4 1") -> "ok" oppure il messaggio d'errore
  if (be_top(vm) < 1 || !be_isstring(vm, 1)) be_raise(vm, "type_error", "act(\"azione\"): serve un testo");
  bool ok = actDo("", be_tostring(vm, 1));
  be_pushstring(vm, ok ? "ok" : g_msg);
  be_return(vm);
}
static int bfLog(bvm* vm) {
  if (be_top(vm) < 1) be_raise(vm, "type_error", "log(testo)");
  vlog("BERRY: %s", be_tostring(vm, 1));
  be_return_nil(vm);
}
static int bfMillis(bvm* vm) { be_pushint(vm, (bint)(millis() & 0x7FFFFFFFUL)); be_return(vm); }
static int bfTime(bvm* vm) { be_pushint(vm, timeValid() ? (bint)time(NULL) : 0); be_return(vm); }      // secondi dal 1970 (0 = ora non valida)
static int bfBtn(bvm* vm) { be_pushint(vm, (bint)btnScriptPop()); be_return(vm); }   // btn(): click liberi del tasto BOOT (1..3), 0 = nessuno; non aspetta
static int bfTemp(bvm* vm) { be_pushreal(vm, (breal)sysCpuTemp()); be_return(vm); }
// logtail(n): ultime n righe del registro (1-20, di fabbrica 10), al massimo 2000 caratteri. Nelle app serve "logread" in use=: il registro puo contenere indirizzi e nomi.
static int bfLogtail(bvm* vm) {
  int n = (be_top(vm) >= 1 && be_isint(vm, 1)) ? (int)be_toint(vm, 1) : 10;
  if (n < 1) n = 1;
  if (n > 20) n = 20;
  static char buf[2001];                                   // nessun String vivo quando si chiama be_pushstring (puo saltare via su memoria finita)
  { String s = logGet(n); size_t l = s.length() > 2000 ? 2000 : s.length(); memcpy(buf, s.c_str() + (s.length() - l), l); buf[l] = 0; }
  be_pushstring(vm, buf);
  be_return(vm);
}
// mqtt("nome", "testo"): pubblica <prefisso>/app/<nome> sul broker. true = inviato, false = non inviato (MQTT non collegato o oltre i 20 messaggi al minuto).
// Solo sotto app/ : un'app non puo scrivere su cmd (i comandi della scheda) ne su status o state.
static uint32_t g_mqWin = 0; static int g_mqCnt = 0;
static int bfMqtt(bvm* vm) {
  if (be_top(vm) < 2 || !be_isstring(vm, 1) || !be_isstring(vm, 2)) be_raise(vm, "type_error", "mqtt(\"nome\", \"testo\"): servono due testi");
  const char* t = be_tostring(vm, 1); const char* m = be_tostring(vm, 2);
  size_t tl = strlen(t); bool okn = tl >= 1 && tl <= 40 && t[0] != '/' && t[tl - 1] != '/' && strlen(m) <= 2048;
  for (size_t i = 0; okn && i < tl; i++) { char c = t[i]; okn = (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '-' || (c == '/' && t[i + 1] != '/'); }
  if (!okn) be_raise(vm, "value_error", "mqtt: nome di 1-40 caratteri (a-z, 0-9, _ - /) e testo fino a 2048");
  uint32_t now = millis();
  if (g_mqCnt == 0 || now - g_mqWin > 60000UL) { g_mqWin = now; g_mqCnt = 0; }
  if (++g_mqCnt > 20) { be_pushbool(vm, false); be_return(vm); }     // oltre il tetto: non inviato (falso), l'app continua
  bool ok = mqttPublishRel(String("app/") + t, String(m));
  be_pushbool(vm, ok);
  be_return(vm);
}
static int bfSleep(bvm* vm) {
  int ms = (be_top(vm) >= 1 && be_isint(vm, 1)) ? (int)be_toint(vm, 1) : 0;
  if (ms <= 0) { vTaskDelay(1); g_lastYield = millis(); }
  while (ms > 0) {
    int d = ms > 20 ? 20 : ms;
    if (d < 10) d = 10;
    vTaskDelay(pdMS_TO_TICKS(d));
    ms -= d;
    if (g_stop) be_raise(vm, "stopped_error", "script fermato");
    if (g_bg) g_lastYield = millis();                      // la pausa conta come respiro: lo script puo girare a lungo
    else if ((int32_t)(millis() - g_deadline) > 0) be_raise(vm, "timeout_error", "script troppo lungo (limite di tempo)");
  }
  be_return_nil(vm);
}

// ---------- funzioni delle APP: dev_get(id), dev_set(id, valore) ----------
// dev_get: numero per switch/slider/button/chart, testo per color/text e per i dati di [data]. dev_set: aggiorna cio che la pagina mostra
// (per chart aggiunge un punto). Nessun String vivo quando si chiama be_raise.
static bool useHas(const char* w);
static char g_ioMsg[100];
// periferica a classi (rele1, led1, pin1, ingresso1, adc1): serve il nome nel use= del manifest. 0 = non e una periferica, 1 = fatto, -1 = errore (testo in g_ioMsg)
static bool useHasDev(const char* nm) {                     // "bmp1.p" (pressione) si autorizza con "bmp1"
  char b[16]; size_t n = strcspn(nm, "."); if (n >= sizeof b) return false;
  memcpy(b, nm, n); b[n] = 0; return useHas(b);
}
static int ioGetFor(const char* nm, long& v) {
  if (!ioHas(nm)) return 0;
  if (!useHasDev(nm)) { snprintf(g_ioMsg, sizeof g_ioMsg, "permesso mancante: aggiungi %s a use= nel manifest", nm); return -1; }
  int x = 0; String e; if (!ioGet(nm, x, e)) { snprintf(g_ioMsg, sizeof g_ioMsg, "%s", e.c_str()); return -1; }
  v = x; return 1;
}
static int ioSetFor(const char* nm, long v) {
  if (!ioHas(nm)) return 0;
  if (!useHas(nm)) { snprintf(g_ioMsg, sizeof g_ioMsg, "permesso mancante: aggiungi %s a use= nel manifest", nm); return -1; }
  String e; if (!ioSet(nm, v != 0, ROLE_ADMIN, e)) { snprintf(g_ioMsg, sizeof g_ioMsg, "%s", e.c_str()); return -1; }   // il permesso lo ha dato l'Admin installando l'app
  return 1;
}
static int bfDevGet(bvm* vm) {
  if (be_top(vm) < 1 || !be_isstring(vm, 1)) be_raise(vm, "type_error", "dev_get(\"id\"): serve il nome di un controllo");
  { long pv = 0; int pr = ioGetFor(be_tostring(vm, 1), pv); if (pr < 0) be_raise(vm, "value_error", g_ioMsg); if (pr > 0) { be_pushint(vm, (bint)pv); be_return(vm); } }
  bool isNum = false, ok; long iv = 0; char sv[RT_TXT + 1];
  ok = rtGet(be_tostring(vm, 1), isNum, iv, sv, sizeof(sv));
  if (!ok) be_raise(vm, "value_error", "id sconosciuto (non e un controllo ne un dato dell'app)");
  if (isNum) be_pushint(vm, (bint)iv); else be_pushstring(vm, sv);
  be_return(vm);
}
static int bfDevSet(bvm* vm) {
  if (be_top(vm) < 2 || !be_isstring(vm, 1)) be_raise(vm, "type_error", "dev_set(\"id\", valore)");
  bool isNum = false; long iv = 0; const char* sv = NULL; const char* why = "";
  if (be_isint(vm, 2)) { isNum = true; iv = (long)be_toint(vm, 2); }
  else if (be_isreal(vm, 2)) { isNum = true; iv = (long)be_toreal(vm, 2); }
  else if (be_isbool(vm, 2)) { isNum = true; iv = be_tobool(vm, 2) ? 1 : 0; }
  else if (be_isstring(vm, 2)) sv = be_tostring(vm, 2);
  else be_raise(vm, "type_error", "dev_set: il valore deve essere un numero o un testo");
  { int pr = isNum ? ioSetFor(be_tostring(vm, 1), iv) : 0; if (pr < 0) be_raise(vm, "value_error", g_ioMsg); if (pr > 0) be_return_nil(vm); }
  if (!rtSetApp(be_tostring(vm, 1), isNum, iv, sv, &why)) be_raise(vm, "value_error", why);
  be_return_nil(vm);
}
static bool useHas(const char* w) {                         // permesso presente nel use= dell'app?
  size_t n = strlen(w); const char* p = g_use;
  while (*p) {
    const char* e = strchr(p, ','); size_t l = e ? (size_t)(e - p) : strlen(p);
    if (l == n && strncmp(p, w, n) == 0) return true;
    if (!e) break; p = e + 1;
  }
  return false;
}
// chiama on_set(id, valore) o on_tick(). 0 = bene. Altro = errore (nella pila resta l'eccezione: la pulisce il chiamante).
static int appCall(bvm* vm, const char* fn, const char* id, bool isNum, long iv, const char* sv) {
  int base = be_top(vm);
  if (!be_getglobal(vm, fn) || !be_isfunction(vm, -1)) { be_pop(vm, be_top(vm) - base); return 0; }   // la funzione non c'e: niente da fare
  int argc = 0;
  if (id) { be_pushstring(vm, id); argc = 2; if (isNum) be_pushint(vm, (bint)iv); else be_pushstring(vm, sv); }
  int r = be_pcall(vm, argc);
  if (r == 0) be_pop(vm, be_top(vm) - base);
  return r;
}
static bool appHas(bvm* vm, const char* fn) {
  int base = be_top(vm); bool ok = be_getglobal(vm, fn) && be_isfunction(vm, -1); be_pop(vm, be_top(vm) - base); return ok;
}
// ciclo dell'app: comandi dalla pagina (on_set) subito, on_tick ogni secondo. Si esce con g_stop o con un errore (che finisce nell'uscita).
static int appLoop(bvm* vm) {
  bool hasSet = appHas(vm, "on_set"), hasTick = appHas(vm, "on_tick");
  if (!hasSet && !hasTick) return 0;                          // nessuna funzione: l'app finisce con il suo codice
  uint32_t lastTick = millis();
  while (!g_stop) {
    char id[13], sv[RT_TXT + 1]; bool isNum; long iv;
    g_lastYield = millis();
    if (rtPop(id, isNum, iv, sv, sizeof(sv))) {
      if (hasSet) { int r = appCall(vm, "on_set", id, isNum, iv, sv); if (r != 0) return r; }
      continue;
    }
    if (hasTick && (int32_t)(millis() - lastTick) >= 1000) {
      lastTick = millis(); g_lastYield = lastTick;
      int r = appCall(vm, "on_tick", NULL, false, 0, NULL); if (r != 0) return r;
    }
    vTaskDelay(pdMS_TO_TICKS(20));
  }
  return 0;
}

// azzera lo stato di una esecuzione (comune a startRun e al passaggio al prossimo script in coda)
static void prepGlobals(bool bg, uint32_t maxMs, const char* name, bool app = false, const char* use = "") {
  g_olen = 0; g_obuf[0] = 0; g_otrunc = false; g_elen = 0; g_ebuf[0] = 0; g_toErr = false;
  g_done = false; g_abandoned = false; g_beats = 0; g_stop = false; g_bg = bg; g_app = app;
  strncpy(g_use, use, sizeof(g_use) - 1); g_use[sizeof(g_use) - 1] = 0;
  strncpy(g_name, name, sizeof(g_name) - 1); g_name[sizeof(g_name) - 1] = 0;
  g_deadline = millis() + maxMs; g_lastYield = millis();
}

// ---------- script salvati in /scripts (.vsc): nome, vecchi script .be di /apps, lettura ----------
static String cleanName(const String& in) {
  String n = in; n.trim(); n.toLowerCase();
  if (n.endsWith(SCRIPT_EXT)) n.remove(n.length() - 4);
  else if (n.endsWith(".be")) n.remove(n.length() - 3);
  return n;
}

// Eredita 1.7.48: gli script stavano in /apps/<nome>.be. Ora /apps e per le app (.vap): li sposto in /scripts/<nome>.vsc.
// Torna quanti ne ha spostati. Non tocca altro.
static bool g_migDone = false;                                          // una volta per avvio: dopo, /apps e delle app
int vosberry::migrateOld() {
  if (g_migDone) return 0;
  g_migDone = true;
  String j = fsListJson("/apps");
  if (j.indexOf("\"ok\":true") < 0) return 0;
  int moved = 0, pos = 0;
  while (moved < 16) {
    int a = j.indexOf("{\"n\":\"", pos); if (a < 0) break;
    int b = j.indexOf("\"", a + 6); if (b < 0) break;
    String name = j.substring(a + 6, b);
    int d = j.indexOf("\"d\":", b);
    bool isDir = d >= 0 && j.substring(d + 4, d + 8) == "true";
    pos = b + 1;
    if (isDir || !name.endsWith(".be")) continue;
    String n = name.substring(0, name.length() - 3);
    if (!utilNameOk(n)) continue;
    String e; fsMkdir(SCRIPT_DIR, e);                                   // se esiste gia, "Esiste gia": non importa
    if (fsRename("/apps/" + name, String(SCRIPT_DIR) + "/" + n + SCRIPT_EXT, e)) { moved++; vlog("BERRY: script %s spostato da /apps a /scripts", n.c_str()); }
  }
  return moved;
}

static bool loadScript(const String& n, String& code, String& err) {   // legge /scripts/<n>.vsc (n gia ripulito e valido)
  String e;
  String path = String(SCRIPT_DIR) + "/" + n + SCRIPT_EXT;
  if (!fsReadText(path, code, e)) {
    if (migrateOld() > 0) fsReadText(path, code, e);                    // forse era un vecchio /apps/<nome>.be
    if (code.length() == 0) { err = String(tr("Script non trovato: scrivi 'script list'")) + " (" + e + ")"; return false; }
  }
  if (code.length() == 0 || code.length() > BERRY_FILE_MAX) { err = tr("Script vuoto o troppo lungo"); return false; }
  return true;
}

// ---------- il task che esegue lo script ----------
static void runTask(void*) {
 for (;;) {
  g_peak = g_used = 0;
  g_lastYield = millis();
  char top; g_stackTop = &top;
  uint32_t t0 = millis();
  bvm* vm = be_vm_new();
  g_runOk = false;
  if (!vm) { strncpy(g_ebuf, "memoria insufficiente per Berry", sizeof(g_ebuf) - 1); g_elen = strlen(g_ebuf); }
  else {
    be_set_obs_hook(vm, obsHook);
    if (!g_app) {
      be_regfunc(vm, "led", bfLed);     be_regfunc(vm, "act", bfAct);       be_regfunc(vm, "log", bfLog);
      be_regfunc(vm, "millis", bfMillis); be_regfunc(vm, "time", bfTime);   be_regfunc(vm, "temp", bfTemp);
      be_regfunc(vm, "sleep", bfSleep);
      be_regfunc(vm, "btn", bfBtn);   btnScriptAttach(true);
      be_regfunc(vm, "logtail", bfLogtail); be_regfunc(vm, "mqtt", bfMqtt);
    } else {                                                   // app: solo cio che il manifest (use=) permette
      be_regfunc(vm, "millis", bfMillis); be_regfunc(vm, "sleep", bfSleep);
      be_regfunc(vm, "dev_get", bfDevGet); be_regfunc(vm, "dev_set", bfDevSet);
      if (useHas("led")) be_regfunc(vm, "led", bfLed);
      if (useHas("act")) be_regfunc(vm, "act", bfAct);
      if (useHas("log")) be_regfunc(vm, "log", bfLog);
      if (useHas("time")) be_regfunc(vm, "time", bfTime);
      if (useHas("temp")) be_regfunc(vm, "temp", bfTemp);
      if (useHas("logread")) be_regfunc(vm, "logtail", bfLogtail);
      if (useHas("mqtt")) be_regfunc(vm, "mqtt", bfMqtt);
      if (useHas("btn")) { be_regfunc(vm, "btn", bfBtn); btnScriptAttach(true); }
    }
    int r = be_loadstring(vm, g_code);
    free(g_code); g_code = NULL;
    if (r == 0) r = be_pcall(vm, 0);
    if (r == 0 && g_app) r = appLoop(vm);
    if (r != 0 && g_app && g_stop) r = 0;                      // l'app e stata fermata a mano: non e un errore
    if (r == BE_MALLOC_FAIL) { g_toErr = true; be_writestring("memory_error: memoria esaurita (tetto di memoria dello script)\n"); g_toErr = false; }   // senza memoria non si puo nemmeno stampare l'errore di Berry
    else if (r != 0) { g_toErr = true; be_dumpexcept(vm); g_toErr = false; }
    g_runOk = (r == 0);
    be_vm_delete(vm);
  }
  btnScriptAttach(false);                                  // fine programma: i click liberi tornano a nessuno
  ledScript(-1);                                           // fine script: il LED torna al modo normale
  g_lastMs = millis() - t0; g_lastPeak = (uint32_t)g_peak; g_lastLeak = (uint32_t)berryFreeAll();   // dopo be_vm_delete di solito non resta nulla
  g_lastOk = g_runOk; g_runs++;
  if (g_bg) {
    char first[81]; size_t k = 0;                          // prima riga dell'errore, per il registro
    while (k < sizeof(first) - 1 && g_ebuf[k] && g_ebuf[k] != '\n') { first[k] = g_ebuf[k]; k++; }
    first[k] = 0;
    if (g_runOk) vlog("BERRY: %s %s finita (%lu ms)", g_app ? "app" : "script", g_name, (unsigned long)g_lastMs);
    else vlog("BERRY: %s %s fermata: %s", g_app ? "app" : "script", g_name, first);
  }
  // script in coda (azione "script <nome>" delle Automazioni): se non e stato fermato a mano si passa al prossimo con lo stesso task
  if (g_bg && !g_stop) {
    char nm[25], who[16]; bool next = false;
    while (qPop(nm, who)) {
      String code, e;
      if (!loadScript(String(nm), code, e)) { vlog("BERRY: script %s dalla coda non avviato: %s", nm, e.c_str()); continue; }
      g_code = (char*)malloc(code.length() + 1);
      if (!g_code) { vlog("BERRY: script %s dalla coda non avviato: memoria", nm); continue; }
      memcpy(g_code, code.c_str(), code.length() + 1);
      prepGlobals(true, 0, nm);
      vlog("BERRY: script %s avviato da %s (dalla coda)", nm, who);
      next = true; break;
    }
    if (next) continue;
  }
  if (g_abandoned || g_bg) { portENTER_CRITICAL(&g_mux); g_busy = false; portEXIT_CRITICAL(&g_mux); }
  g_done = true;
  vTaskDelete(NULL);
  return;
 }
}

// avvia il task (comune a "berry <codice>" e agli script di /scripts)
static bool startRun(const String& code, bool bg, uint32_t maxMs, const char* name, String& err, bool app = false, const char* use = "") {
  if (heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL) < 24000) { err = tr("RAM interna troppo bassa per avviare uno script"); return false; }
  bool was;
  portENTER_CRITICAL(&g_mux); was = g_busy; g_busy = true; portEXIT_CRITICAL(&g_mux);
  if (was) { err = tr("Berry e occupato con un altro script"); return false; }
  g_code = (char*)malloc(code.length() + 1);
  if (!g_code) { g_busy = false; err = tr("RAM interna troppo bassa per avviare uno script"); return false; }
  memcpy(g_code, code.c_str(), code.length() + 1);
  prepGlobals(bg, maxMs, name, app, use);
  if (xTaskCreatePinnedToCore(runTask, "berry", BERRY_STACK_SIZE, NULL, 1, NULL, 1) != pdPASS) {
    free(g_code); g_code = NULL; g_busy = false; err = tr("Non riesco ad avviare lo script"); return false;
  }
  return true;
}

bool berryEval(const String& code, String& out, String& err, uint32_t maxMs) {
  out = ""; err = "";
  if (code.length() == 0 || code.length() > BERRY_CODE_MAX) { err = tr("Script vuoto o troppo lungo"); return false; }
  if (!startRun(code, false, maxMs, "", err)) return false;
  uint32_t t0 = millis();
  while (!g_done && millis() - t0 < maxMs + 2500UL) vTaskDelay(pdMS_TO_TICKS(10));
  if (!g_done) {                                           // non risponde: il task liberera da solo il "busy"
    portENTER_CRITICAL(&g_mux); g_abandoned = true; portEXIT_CRITICAL(&g_mux);
    if (g_done) g_busy = false;                            // finito proprio adesso
    err = tr("Lo script non risponde");
    return false;
  }
  out = String(g_obuf); if (g_otrunc) out += "\n...";
  err = String(g_ebuf);
  bool ok = g_runOk;
  g_busy = false;
  return ok;
}

bool berryNameValid(const String& nameIn) { return utilNameOk(cleanName(nameIn)); }

bool berryStart(const String& nameIn, String& err) {
  err = "";
  String n = cleanName(nameIn);
  if (!utilNameOk(n)) { err = tr("Nome script non valido (1-24 caratteri: lettere minuscole, numeri, trattino, sottotrattino)"); return false; }
  String code;
  if (!loadScript(n, code, err)) return false;
  if (!startRun(code, true, 0, n.c_str(), err)) return false;
  vlog("BERRY: script %s avviato (%u caratteri)", n.c_str(), (unsigned)code.length());
  return true;
}

// Avvio da una regola (o da MQTT / rete schede): parte subito se Berry e libero, altrimenti va in coda (massimo 4, senza doppioni).
bool berryQueue(const String& nameIn, String& err, const char* who) {
  err = "";
  String n = cleanName(nameIn);
  if (!utilNameOk(n)) { err = tr("Nome script non valido (1-24 caratteri: lettere minuscole, numeri, trattino, sottotrattino)"); return false; }
  bool busy; portENTER_CRITICAL(&g_mux); busy = g_busy; portEXIT_CRITICAL(&g_mux);
  if (!busy) {
    String code;
    if (!loadScript(n, code, err)) return false;
    if (startRun(code, true, 0, n.c_str(), err)) { vlog("BERRY: script %s avviato da %s (%u caratteri)", n.c_str(), who, (unsigned)code.length()); return true; }
    if (!g_busy) return false;                                        // errore vero (RAM, task): non si accoda
    err = "";                                                         // nel frattempo si e occupato: in coda
  }
  bool full = false, dup = false;
  portENTER_CRITICAL(&g_mux);
  for (int i = 0; i < g_qn; i++) if (n == g_q[i]) dup = true;
  if (!dup) {
    if (g_qn >= BERRY_QMAX) full = true;
    else { strncpy(g_q[g_qn], n.c_str(), 24); g_q[g_qn][24] = 0; strncpy(g_qwho[g_qn], who ? who : "?", 15); g_qwho[g_qn][15] = 0; g_qn++; }
  }
  portEXIT_CRITICAL(&g_mux);
  if (dup) { err = tr("Script gia in coda"); return false; }
  if (full) { err = tr("Coda piena (massimo 4 script in attesa)"); return false; }
  vlog("BERRY: script %s in coda (richiesto da %s)", n.c_str(), who);
  return true;
}

// Avvia il codice di un'app (gia letta e controllata da vos_app.cpp) con i permessi del suo manifest. Torna subito.
bool berryStartApp(const String& name, const String& code, const String& use, String& err) {
  err = "";
  if (code.length() == 0 || code.length() > BERRY_FILE_MAX) { err = tr("Script vuoto o troppo lungo"); return false; }
  if (!startRun(code, true, 0, name.c_str(), err, true, use.c_str())) return false;
  vlog("BERRY: app %s avviata (%u caratteri)", name.c_str(), (unsigned)code.length());
  return true;
}
#endif
