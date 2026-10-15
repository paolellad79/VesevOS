// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_berry_scripts.cpp: elenco degli script, stato, uscita, stop (vedi vos_berry.cpp).
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
#include "../devices/vos_led.h"
#include "vos_btn.h"
#include "vos_mqtt.h"
#include "vos_berry_int.h"

bool berryLastIsApp(const String& name) { return g_app && name == String(g_name); }       // l'ultima esecuzione (o quella in corso) e questa app?
bool berryAppRunning(String& name) { if (g_busy && g_app) { name = String(g_name); return true; } return false; }
// uscita e errore dell'ultima esecuzione (copie sicure) per la pagina e il comando "app out"
String berryAppOutJson() {
  bool busy = g_busy;
  char o[BERRY_OUT_MAX + 1], e[sizeof(g_ebuf)];
  size_t on = g_olen; if (on > BERRY_OUT_MAX) on = BERRY_OUT_MAX; memcpy(o, g_obuf, on); o[on] = 0;
  size_t en = busy ? 0 : g_elen; if (en > sizeof(e) - 1) en = sizeof(e) - 1; memcpy(e, g_ebuf, en); e[en] = 0;
  return String("\"out\":\"") + jsonEscape(String(o)) + "\",\"err\":\"" + jsonEscape(String(e)) + "\",\"ms\":" + String((unsigned long)g_lastMs) + ",\"last\":" + (g_runs ? (g_lastOk ? "1" : "0") : "-1");
}
bool berryStop(String& err) {
  err = "";
  if (!g_busy) { err = tr("Nessuno script in corso"); return false; }
  qClear();                                                           // fermare a mano toglie anche gli script in coda
  g_stop = true;
  uint32_t t0 = millis();
  while (g_busy && millis() - t0 < 2500UL) vTaskDelay(pdMS_TO_TICKS(10));
  if (g_busy) { err = tr("Lo script non risponde"); return false; }
  return true;
}

bool berryRunning() { return g_busy; }

// scorre gli script di /scripts (solo file .vsc): chiama cb(nome, byte, ctx). Crea la cartella se manca; sposta i vecchi /apps/<nome>.be.
static int scanScripts(void (*cb)(const String&, long, void*), void* ctx) {
  migrateOld();
  String j = fsListJson(SCRIPT_DIR);
  if (j.indexOf("\"ok\":true") < 0) {                       // cartella mancante: la creo
    String e; fsMkdir(SCRIPT_DIR, e);
    j = fsListJson(SCRIPT_DIR);
  }
  int n = 0, pos = 0;
  while (true) {
    int a = j.indexOf("{\"n\":\"", pos); if (a < 0) break;
    int b = j.indexOf("\"", a + 6); if (b < 0) break;
    String name = j.substring(a + 6, b);
    int d = j.indexOf("\"d\":", b), sz = j.indexOf("\"s\":", b);
    bool isDir = d >= 0 && j.substring(d + 4, d + 8) == "true";
    long size = sz >= 0 ? j.substring(sz + 4).toInt() : 0;
    pos = b + 1;
    if (isDir || !name.endsWith(SCRIPT_EXT)) continue;
    cb(name.substring(0, name.length() - 4), size, ctx); n++;
  }
  return n;
}

struct ListCtx { String s; };
static void listCb(const String& nm, long size, void* c) {
  ListCtx* x = (ListCtx*)c;
  x->s += "  " + nm + "  (" + String(size) + " B)";
  if (g_busy && g_bg && nm == String(g_name)) x->s += String("  <- ") + tr("in corso");
  x->s += "\n";
}
String berryList() {
  ListCtx x; int n = scanScripts(listCb, &x);
  if (!n) return String(tr("Nessuno script in /scripts. Esempio: write /scripts/prova.vsc print('ciao')")) + "\n";
  return String(trf("Script in /scripts: %d", n)) + "\n" + x.s;
}

struct JsonCtx { String s; };
static void jsonCb(const String& nm, long size, void* c) {
  JsonCtx* x = (JsonCtx*)c;
  if (x->s.length()) x->s += ",";
  x->s += "{\"n\":\"" + nm + "\",\"s\":" + String(size) + "}";               // il nome ha solo a-z 0-9 - _ (altri nomi non compaiono)
}
String berryStateJson() {
  JsonCtx x;
  struct Nm { static void cb(const String& nm, long size, void* c) { if (utilNameOk(nm)) jsonCb(nm, size, c); } };
  scanScripts(Nm::cb, &x);
  bool busy = g_busy, bg = g_bg;
  char o[BERRY_OUT_MAX + 1], e[sizeof(g_ebuf)];                       // copie: il task puo scrivere mentre leggo
  size_t on = g_olen; if (on > BERRY_OUT_MAX) on = BERRY_OUT_MAX; memcpy(o, g_obuf, on); o[on] = 0;
  size_t en = (busy && bg) ? 0 : g_elen; if (en > sizeof(e) - 1) en = sizeof(e) - 1; memcpy(e, g_ebuf, en); e[en] = 0;
  String j = String("{\"ok\":true,\"ver\":\"") + BERRY_VERSION + "\",\"busy\":" + (busy ? "true" : "false") + ",\"bg\":" + (bg ? "true" : "false") +
             ",\"name\":\"" + String(g_name) + "\",\"list\":[" + x.s + "],\"out\":\"" + jsonEscape(String(o)) + "\",\"err\":\"" + jsonEscape(String(e)) +
             "\",\"trunc\":" + (g_otrunc ? "true" : "false") + ",\"max\":" + String((unsigned)BERRY_FILE_MAX) + ",\"q\":" + String(g_qn) + ",\"runs\":" + String((unsigned long)g_runs) +
             ",\"last\":" + (g_runs ? (g_lastOk ? "1" : "0") : "-1") + ",\"ms\":" + String((unsigned long)g_lastMs) + "}";
  return j;
}

String berryOutText() {
  String s;
  if (g_name[0]) s += String(trf("Script: %s", g_name)) + (g_busy && g_bg ? String("  (") + tr("in corso") + ")" : String("")) + "\n";
  if (g_olen) { s += String(g_obuf); if (g_otrunc) s += String("\n... (") + tr("le righe piu vecchie sono state tolte") + ")"; if (!s.endsWith("\n")) s += "\n"; }
  if (g_elen && !(g_busy && g_bg)) s += String(g_ebuf) + (String(g_ebuf).endsWith("\n") ? "" : "\n");
  if (!g_olen && !g_elen) s += String(tr("Nessuna uscita")) + "\n";
  return s;
}

String berryStatusText() {
  String s = String("Berry ") + BERRY_VERSION + " " + String(trf("(tetto memoria %lu KB, tempo %lu s)", (unsigned long)(BERRY_MEM_LIMIT / 1024), (unsigned long)(BERRY_TIME_MS / 1000))) + "\n";
  if (g_busy) s += String(g_bg ? trf("In corso: %s", g_name) : String(tr("In corso: comando"))) + "\n";
  s += String(trf("Esecuzioni: %lu", (unsigned long)g_runs)) + "\n";
  if (g_runs) s += String(trf("Ultima: %s, %lu ms, memoria massima %lu byte, non liberata %lu byte", g_lastOk ? "OK" : tr("Errore"), (unsigned long)g_lastMs, (unsigned long)g_lastPeak, (unsigned long)g_lastLeak)) + "\n";
  s += String(tr("Funzioni: print, led, act, time, millis, temp, log, sleep, btn")) + "\n";
  s += String(tr("Comandi: berry <codice> | script list | script run <nome> | script stop | script out")) + "\n";
  return s;
}
#endif
