// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_btn.cpp
#include "vos_btn.h"
#include "../core/vos_config.h"
#include "../core/vos_eventbus.h"
#include "../core/vos_log.h"
#include "../core/vos_i18n.h"
#include "../core/vos_util.h"
#include "../devices/vos_led.h"
#include "../net/vos_net.h"
#include "../net/vos_wps.h"

static uint8_t  g_clicks = 0;
static uint32_t g_last = 0;
static uint32_t g_testUntil = 0, g_downSince = 0, g_seq = 0;
static bool     g_down = false;
#define BTN_Q 4
static volatile uint8_t g_q[BTN_Q];                 // coda dei click liberi (chi scrive: loop; chi legge: task Berry)
static volatile uint8_t g_qh = 0, g_qt = 0;
static volatile bool    g_script = false;
static String   g_lastTxt;

bool btnTestActive() { return g_testUntil && (int32_t)(g_testUntil - millis()) > 0; }
void btnTestSet(bool on) { g_testUntil = on ? (millis() + BTN_TEST_MS) : 0; if (on && !g_testUntil) g_testUntil = 1; g_clicks = 0; g_lastTxt = ""; g_seq++; }
static void testNote(const String& t) { g_lastTxt = t; g_seq++; }

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

bool btnSetAction(int n, int a, String& err) {
  if (n < 1 || n > BTN_CLICKS || a < 0 || a >= BA_MAX) { err = tr("Valore non valido"); return false; }
  cfg.btnAct[n - 1] = (uint8_t)a;
  return true;
}

static void run(int n) {
  uint8_t a = btnAction(n);
  if (btnTestActive()) { testNote(trf("%d click: %s", n, btnActName(a)) + " - " + tr("prova: non eseguito")); ledIdentify(150); return; }
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
    String e; if (!wpsStart(false, "tasto BOOT", e)) vlog("WPS: %s", e.c_str());
  } else if (a == BA_FIND) {
    ledIdentify(10000);
  }
}

void btnClick() {
  if (g_clicks < 255) g_clicks++;
  g_last = millis();
  if (g_clicks >= BTN_CLICKS) { g_clicks = 0; run(BTN_CLICKS); }   // 3 click: piu di cosi non si gestisce, subito (altri click: nuovo gesto)
}

void btnLongRelease() { g_clicks = 0; }

void btnTestLong(uint32_t ms) {
  const char* w = ms >= 20000 ? tr("ripristino di fabbrica") : ms >= 8000 ? tr("azzera le password") : tr("spegne il filtro IP");
  testNote(trf("%d s: %s", (int)(ms / 1000), w) + " - " + tr("prova: non eseguito"));
}

void btnTick(bool down) {
  if (down && !g_down) g_downSince = millis();
  g_down = down;
  if (g_testUntil && !btnTestActive()) { g_testUntil = 0; g_seq++; }   // fine del tempo
  if (!g_clicks || down) return;
  if ((uint32_t)(millis() - g_last) < BTN_GAP_MS) return;
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

String btnJson() {
  String j = "{\"act\":[";
  for (int n = 1; n <= BTN_CLICKS; n++) { if (n > 1) j += ","; j += String((int)btnAction(n)); }
  j += "],\"opt\":[";
  for (int a = 0; a < BA_MAX; a++) { if (a) j += ","; j += "{\"id\":" + String(a) + ",\"label\":\"" + jsonEscape(btnActName(a)) + "\"}"; }
  int left = btnTestActive() ? (int)(((int32_t)(g_testUntil - millis()) + 999) / 1000) : 0;
  j += "],\"free\":[";
  for (int n = 1; n <= BTN_CLICKS; n++) { if (n > 1) j += ","; j += btnScriptFree(n) ? "true" : "false"; }
  j += "],\"script\":" + String(g_script ? "true" : "false") + ",\"test\":" + String(left) + ",\"down\":" + String(g_down ? "true" : "false") + ",\"heldMs\":" + String(g_down ? (unsigned long)(millis() - g_downSince) : 0UL) +
       ",\"seq\":" + String((unsigned long)g_seq) + ",\"last\":\"" + jsonEscape(g_lastTxt) + "\",\"wpsClicks\":" + String(btnWpsClicks()) + ",\"wpsOn\":" + String(wpsAllowed() ? "true" : "false") + "}";
  return j;
}

String btnText() {
  String t;
  for (int n = 1; n <= BTN_CLICKS; n++) t += trf("%d click: %s", n, btnActName(btnAction(n))) + "\n";
  return t;
}
