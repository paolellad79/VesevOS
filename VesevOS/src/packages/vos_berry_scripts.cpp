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
#include <stdlib.h>
#include <string.h>
#include <string_view>
#include "esp_heap_caps.h"
#include "vos_rules.h"
#include "vos_app_rt.h"
#include "../core/vos_log.h"
#include "../core/vos_time.h"
#include "../core/vos_sys.h"
#include "../core/vos_i18n.h"
#include "../core/vos_files.h"
#include "../core/vos_util.h"
#include "../core/vos_parse.h"
#include "../devices/vos_led.h"
#include "vos_btn.h"
#include "vos_mqtt.h"
#include "vos_berry_int.h"
#include "../drivers/vos_drv_time.h"

bool berryLastIsApp(const char* name) { return g_app && strcmp(name, g_name) == 0; }       // l'ultima esecuzione (o quella in corso) e questa app?
bool berryAppRunning(char* name, size_t n) { if (g_busy && g_app) { snprintf(name, n, "%s", g_name); return true; } return false; }

// Copia di uscita e errore dell'ultima esecuzione: il task Berry puo scrivere mentre leggo, quindi si copia sotto g_mux (pochi
// microsecondi, solo memcpy) e poi si lavora sulla copia. La copia sta sull'heap: sullo stack del web server non c'e posto (1,8 KB).
namespace {
struct Snap {
  char* buf = nullptr;                 // un solo blocco: uscita, poi errore (ognuno con il suo zero finale)
  char* o = nullptr; char* e = nullptr;
  size_t on = 0, en = 0;
  explicit Snap(bool hideErr) {
    buf = (char*)malloc(BERRY_OUT_MAX + 1 + BERRY_EBUF_SZ);
    if (!buf) return;
    o = buf; e = buf + BERRY_OUT_MAX + 1;
    portENTER_CRITICAL(&g_mux);
    on = g_olen; if (on > BERRY_OUT_MAX) on = BERRY_OUT_MAX; memcpy(o, g_obuf, on);
    en = hideErr ? 0 : g_elen; if (en > BERRY_EBUF_SZ - 1) en = BERRY_EBUF_SZ - 1; memcpy(e, g_ebuf, en);
    portEXIT_CRITICAL(&g_mux);
    o[on] = 0; e[en] = 0;
  }
  ~Snap() { free(buf); }
  Snap(const Snap&) = delete;
  Snap& operator=(const Snap&) = delete;
  bool ok() const { return buf != nullptr; }
};
bool full(const HeapBuf& b) { return b.over() || b.oom(); }
void addLong(HeapBuf& b, long v) { char t[16]; int n = snprintf(t, sizeof t, "%ld", v); b.add(t, n > 0 ? (size_t)n : 0); }
}  // namespace

// uscita e errore dell'ultima esecuzione per la pagina e il comando "app out"
bool berryAppOutJson(HeapBuf& o) {
  bool busy = g_busy;
  Snap s(busy);
  if (!s.ok()) return false;
  o.add("\"out\":\""); o.addJson(s.o, s.on);
  o.add("\",\"err\":\""); o.addJson(s.e, s.en);
  o.add("\",\"ms\":"); o.addUint(g_lastMs);
  o.add(",\"last\":"); o.add(g_runs ? (g_lastOk ? "1" : "0") : "-1");
  return !full(o);
}
bool berryStop(ErrBuf& err) {
  err = "";
  if (!g_busy) { err = tr("Nessuno script in corso"); return false; }
  qClear();                                                           // fermare a mano toglie anche gli script in coda
  g_stop = true;
  uint32_t t0 = drvMillis();
  while (g_busy && drvMillis() - t0 < 2500UL) vTaskDelay(pdMS_TO_TICKS(10));
  if (g_busy) { err = tr("Lo script non risponde"); return false; }
  return true;
}

bool berryRunning() { return g_busy; }

// scorre gli script di /scripts (solo file .vsc): chiama cb(nome, byte, ctx). Crea la cartella se manca; sposta i vecchi /apps/<nome>.be.
// L'elenco dei file e un JSON {"n":"nome","d":true|false,"s":byte}: si legge con viste, senza copiare i nomi.
typedef void (*ScriptCb)(std::string_view name, long size, void* ctx);
static int scanScripts(ScriptCb cb, void* ctx) {
  migrateOld();
  HeapBuf j(FS_JSON_MAX);
  if (!fsListBuild(SCRIPT_DIR, j)) return 0;                // manca la memoria
  if (!strstr(j.c_str(), "\"ok\":true")) {                  // cartella mancante: la creo
    ErrBuf e; fsMkdir(SCRIPT_DIR, e);
    j.clear();
    if (!fsListBuild(SCRIPT_DIR, j)) return 0;
  }
  std::string_view v(j.c_str(), j.length());
  const size_t none = std::string_view::npos;
  int n = 0; size_t pos = 0;
  while (true) {
    size_t a = v.find("{\"n\":\"", pos); if (a == none) break;
    size_t b = v.find('"', a + 6); if (b == none) break;
    std::string_view name = v.substr(a + 6, b - (a + 6));
    size_t d = v.find("\"d\":", b), sz = v.find("\"s\":", b);
    bool isDir = d != none && v.substr(d + 4, 4) == "true";
    long size = sz != none ? viewToLong(v.substr(sz + 4)) : 0;
    pos = b + 1;
    if (isDir || name.size() < 4 || name.substr(name.size() - 4) != SCRIPT_EXT) continue;
    cb(name.substr(0, name.size() - 4), size, ctx); n++;
  }
  return n;
}

static void listCb(std::string_view nm, long size, void* c) {
  HeapBuf& b = *(HeapBuf*)c;
  b.add("  "); b.add(nm.data(), nm.size()); b.add("  ("); addLong(b, size); b.add(" B)");
  if (g_busy && g_bg && nm == std::string_view(g_name)) { b.add("  <- "); b.add(tr("in corso")); }
  b.add('\n');
}
void berryList(HeapBuf& o) {
  HeapBuf body(BERRY_TEXT_MAX);
  int n = scanScripts(listCb, &body);
  if (!n) { o.add(tr("Nessuno script in /scripts. Esempio: write /scripts/prova.vsc print('ciao')")); o.add('\n'); return; }
  o.add(trf("Script in /scripts: %d", n).c_str()); o.add('\n');
  o.add(body.c_str(), body.length());
}

// elenco per la pagina: {"n":"nome","s":byte}, separati da virgole (il nome ha solo a-z 0-9 - _: altri nomi non compaiono)
struct JsonCtx { HeapBuf* b; bool first; };
static void jsonCb(std::string_view nm, long size, void* c) {
  JsonCtx& x = *(JsonCtx*)c;
  if (!utilNameOk(nm.data(), nm.size())) return;
  if (!x.first) x.b->add(',');
  x.first = false;
  x.b->add("{\"n\":\""); x.b->add(nm.data(), nm.size()); x.b->add("\",\"s\":"); addLong(*x.b, size); x.b->add('}');
}
bool berryStateJson(HeapBuf& o) {
  bool busy = g_busy, bg = g_bg;
  Snap s(busy && bg);
  if (!s.ok()) return false;
  o.add("{\"ok\":true,\"ver\":\""); o.add(BERRY_VERSION);
  o.add("\",\"busy\":"); o.add(busy ? "true" : "false");
  o.add(",\"bg\":"); o.add(bg ? "true" : "false");
  o.add(",\"name\":\""); o.addJson(g_name, strlen(g_name));
  o.add("\",\"list\":[");
  JsonCtx x{&o, true};
  scanScripts(jsonCb, &x);
  o.add("],\"out\":\""); o.addJson(s.o, s.on);
  o.add("\",\"err\":\""); o.addJson(s.e, s.en);
  o.add("\",\"trunc\":"); o.add(g_otrunc ? "true" : "false");
  o.add(",\"max\":"); o.addUint((unsigned)BERRY_FILE_MAX);
  o.add(",\"q\":"); addLong(o, g_qn);
  o.add(",\"runs\":"); o.addUint(g_runs);
  o.add(",\"last\":"); o.add(g_runs ? (g_lastOk ? "1" : "0") : "-1");
  o.add(",\"ms\":"); o.addUint(g_lastMs);
  o.add('}');
  return !full(o);
}

void berryOutText(HeapBuf& o) {
  bool busyBg = g_busy && g_bg;
  Snap s(busyBg);
  if (g_name[0]) {
    o.add(trf("Script: %s", g_name).c_str());
    if (busyBg) { o.add("  ("); o.add(tr("in corso")); o.add(')'); }
    o.add('\n');
  }
  if (s.ok() && s.on) {
    o.add(s.o, s.on);
    if (g_otrunc) { o.add("\n... ("); o.add(tr("le righe piu vecchie sono state tolte")); o.add(')'); }
    if (o.length() && o.c_str()[o.length() - 1] != '\n') o.add('\n');
  }
  if (s.ok() && s.en) { o.add(s.e, s.en); if (s.e[s.en - 1] != '\n') o.add('\n'); }
  if (!s.ok() || (!s.on && !s.en && !g_elen)) { o.add(tr("Nessuna uscita")); o.add('\n'); }
}

void berryStatusText(HeapBuf& o) {
  o.add("Berry "); o.add(BERRY_VERSION); o.add(' ');
  o.add(trf("(tetto memoria %lu KB, tempo %lu s)", (unsigned long)(BERRY_MEM_LIMIT / 1024), (unsigned long)(BERRY_TIME_MS / 1000)).c_str()); o.add('\n');
  if (g_busy) { o.add(g_bg ? trf("In corso: %s", g_name).c_str() : tr("In corso: comando")); o.add('\n'); }
  o.add(trf("Esecuzioni: %lu", (unsigned long)g_runs).c_str()); o.add('\n');
  if (g_runs) { o.add(trf("Ultima: %s, %lu ms, memoria massima %lu byte, non liberata %lu byte", g_lastOk ? "OK" : tr("Errore"), (unsigned long)g_lastMs, (unsigned long)g_lastPeak, (unsigned long)g_lastLeak).c_str()); o.add('\n'); }
  o.add(tr("Funzioni: print, led, act, time, millis, temp, log, sleep, btn")); o.add('\n');
  o.add(tr("Comandi: berry <codice> | script list | script run <nome> | script stop | script out")); o.add('\n');
}
#endif
