// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_btn.cpp
#include "vos_btn.h"
#include "../core/vos_config.h"
#include "../core/vos_eventbus.h"
#include "../core/vos_log.h"
#include "../core/vos_i18n.h"
#include "../core/vos_strbuf.h"
#include "../devices/vos_led.h"
#include "../net/vos_net.h"
#include "../net/vos_wps.h"
#include "../drivers/vos_drv_time.h"

static uint8_t  g_clicks = 0;
static uint32_t g_last = 0;
static uint32_t g_testUntil = 0, g_downSince = 0, g_seq = 0;
static bool     g_down = false;
#define BTN_Q 4
static volatile uint8_t g_q[BTN_Q];                 // coda dei click liberi (chi scrive: loop; chi legge: task Berry)
static volatile uint8_t g_qh = 0, g_qt = 0;
static volatile bool    g_script = false;
static char     g_lastTxt[96];                      // ultima nota del modo prova (si tronca se piu lunga)

bool btnTestActive() { return g_testUntil && (int32_t)(g_testUntil - drvMillis()) > 0; }
void btnTestSet(bool on) { g_testUntil = on ? (drvMillis() + BTN_TEST_MS) : 0; if (on && !g_testUntil) g_testUntil = 1; g_clicks = 0; g_lastTxt[0] = 0; g_seq++; }
// "<gesto> - prova: non eseguito"
static void testNote(const char* gesture) {
  StrBuf(g_lastTxt, sizeof(g_lastTxt)).add(gesture).add(" - ").add(tr("prova: non eseguito"));
  g_seq++;
}

uint8_t btnAction(int n) { return (n >= 1 && n <= BTN_CLICKS) ? cfg.btnAct[n - 1] : (uint8_t)BA_NONE; }

const char* btnActName(int a) {
  switch (a) {
    case BA_AIR_OFF: return tr("Esce dal modo aereo");
    case BA_WPS:     return tr("Avvia o annulla il WPS");
    case BA_FIND:    return tr("Fa l'arcobaleno sul LED (trova la scheda)");
    default:         return tr("Libero (lo usano gli script)");
  }
}

int btnWpsClicks() {
  for (int n = 1; n <= BTN_CLICKS; n++) if (btnAction(n) == BA_WPS) return n;
  return 0;
}

bool btnSetAction(int n, int a, ErrBuf& err) {
  if (n < 1 || n > BTN_CLICKS || a < 0 || a >= BA_MAX) { err = tr("Valore non valido"); return false; }
  cfg.btnAct[n - 1] = (uint8_t)a;
  return true;
}

static void run(int n) {
  uint8_t a = btnAction(n);
  if (btnTestActive()) {
    char w[64]; StrBuf(w, sizeof(w)).addf(tr("%d click: %s"), n, btnActName(a));
    testNote(w); ledIdentify(150); return;
  }
  EventBus::publish("btn.click", btnActName(a), n);
  if (a == BA_NONE) {                                 // libero: va al programma che lo chiede
    if (!g_script) return;
    uint8_t h = g_qh; if ((uint8_t)((h + 1) % BTN_Q) == g_qt) return;   // coda piena: si perde
    g_q[h] = (uint8_t)n; g_qh = (uint8_t)((h + 1) % BTN_Q);
    ledIdentify(150); return;
  }
  ledIdentify(150);                                   // lampo: il gesto e stato capito
  if (a == BA_AIR_OFF) {
    if (netAirplane()) netAirplaneOff("tasto BOOT");
  } else if (a == BA_WPS) {
    if (wpsActive()) { wpsStop(); return; }
    if (!wpsAllowed()) { vlog("WPS: spento (accendilo in Rete > WPS)"); return; }
    ErrBuf e; if (!wpsStart(false, "tasto BOOT", e)) vlog("WPS: %s", e.c_str());
  } else if (a == BA_FIND) {
    ledIdentify(10000);
  }
}

void btnClick() {
  if (g_clicks < 255) g_clicks++;
  g_last = drvMillis();
  if (g_clicks >= BTN_CLICKS) { g_clicks = 0; run(BTN_CLICKS); }   // 3 click: piu di cosi non si gestisce, subito (altri click: nuovo gesto)
}

void btnLongRelease() { g_clicks = 0; }

void btnTestLong(uint32_t ms) {
  const char* w = ms >= 20000 ? tr("ripristino di fabbrica") : ms >= 8000 ? tr("azzera le password") : tr("spegne il filtro IP");
  char b[64]; StrBuf(b, sizeof(b)).addf(tr("%d s: %s"), (int)(ms / 1000), w);
  testNote(b);
}

void btnTick(bool down) {
  if (down && !g_down) g_downSince = drvMillis();
  g_down = down;
  if (g_testUntil && !btnTestActive()) { g_testUntil = 0; g_seq++; }   // fine del tempo
  if (!g_clicks || down) return;
  if ((uint32_t)(drvMillis() - g_last) < BTN_GAP_MS) return;
  int n = g_clicks; g_clicks = 0;
  if (n <= BTN_CLICKS) run(n);
}

void btnScriptAttach(bool on) { g_qt = g_qh; g_script = on; }
bool btnScriptFree(int n) { return btnAction(n) == BA_NONE; }
int btnScriptPop() {
  if (g_qt == g_qh) return 0;
  uint8_t v = g_q[g_qt]; g_qt = (uint8_t)((g_qt + 1) % BTN_Q);
  return v;
}

bool btnJson(HeapBuf& j) {
  j.add("{\"act\":[");
  for (int n = 1; n <= BTN_CLICKS; n++) j.addf(n > 1 ? ",%d" : "%d", (int)btnAction(n));
  j.add("],\"opt\":[");
  for (int a = 0; a < BA_MAX; a++) {
    j.addf(a ? ",{\"id\":%d,\"label\":\"" : "{\"id\":%d,\"label\":\"", a);
    j.addJson(btnActName(a));
    j.add("\"}");
  }
  int left = btnTestActive() ? (int)(((int32_t)(g_testUntil - drvMillis()) + 999) / 1000) : 0;
  j.add("],\"free\":[");
  for (int n = 1; n <= BTN_CLICKS; n++) j.addf(n > 1 ? ",%s" : "%s", btnScriptFree(n) ? "true" : "false");
  j.addf("],\"script\":%s,\"test\":%d,\"down\":%s,\"heldMs\":%lu,\"seq\":%lu,\"last\":\"",
         g_script ? "true" : "false", left, g_down ? "true" : "false", g_down ? (unsigned long)(drvMillis() - g_downSince) : 0UL, (unsigned long)g_seq);
  j.addJson(g_lastTxt);
  return j.addf("\",\"wpsClicks\":%d,\"wpsOn\":%s}", btnWpsClicks(), wpsAllowed() ? "true" : "false");
}

// una riga per gesto (al massimo ~100 byte l'una, tradotta): 512 bastano per i 3 click
std::string btnText() {
  StrText t(512);
  for (int n = 1; n <= BTN_CLICKS; n++) t.buf().addf(tr("%d click: %s"), n, btnActName(btnAction(n))).add('\n');
  return t.take();
}
