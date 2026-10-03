// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_boot.cpp
// I servizi "mobili" partono nell'ordine scelto dall'utente. Ogni servizio dichiara cosa richiede:
// l'ordine finale e sempre corretto in modo che ognuno parta DOPO quelli che richiede.
// I servizi fissi (log, pin, file/config, lingua) partono sempre per primi, in setup().
#include "vos_boot.h"
#include "vos_util.h"
#include "vos_log.h"
#include "vos_sys.h"
#include "vos_led.h"
#include "vos_net.h"
#include "vos_time.h"
#include "vos_web.h"
#include "vos_rules.h"
#include "vos_mqtt.h"
#include <Preferences.h>

#define NSVC 7

struct Svc {
  const char* id;
  void (*start)();
  uint8_t req;       // maschera dei servizi richiesti
  uint8_t parent;    // genitore nella vista ad albero (255 = nessuno)
};

enum { S_SYS = 0, S_LED, S_NET, S_TIME, S_WEB, S_RULES, S_MQTT };

static const Svc SV[NSVC] = {
  { "sys",   sysInit,   0,                                  255 },
  { "led",   ledInit,   0,                                  255 },
  { "net",   netInit,   0,                                  255 },
  { "time",  timeInit,  (uint8_t)(1 << S_NET),              S_NET },
  { "web",   webInit,   (uint8_t)((1 << S_NET) | (1 << S_SYS)), S_NET },
  { "rules", rulesInit, (uint8_t)((1 << S_TIME) | (1 << S_SYS)), S_TIME },
  { "mqtt",  mqttInit,  (uint8_t)((1 << S_NET) | (1 << S_SYS)), S_NET },
};

static const char* DEF_ORDER = "sys,led,net,time,web,rules,mqtt";

static int   g_ord[NSVC];
static uint32_t g_at[NSVC], g_ms[NSVC];
static bool  g_custom = false, g_fell = false, g_stable = false;
static uint8_t g_try = 0;

static int svcIndex(const String& id) {
  for (int i = 0; i < NSVC; i++) if (id == SV[i].id) return i;
  return -1;
}

// csv -> ordine finale (indici). Nomi sconosciuti o doppi si scartano, i mancanti si aggiungono.
static void sortOrder(const String& csv, int* out) {
  int list[NSVC], n = 0;
  uint8_t seen = 0;
  int p = 0;
  while (p <= (int)csv.length() && n < NSVC) {
    int e = csv.indexOf(',', p);
    if (e < 0) e = csv.length();
    String id = csv.substring(p, e); id.trim();
    int k = svcIndex(id);
    if (k >= 0 && !(seen & (1 << k))) { list[n++] = k; seen |= (1 << k); }
    p = e + 1;
  }
  for (int i = 0; i < NSVC; i++) if (!(seen & (1 << i))) list[n++] = i;
  // ordinamento stabile: ogni servizio dopo quelli che richiede
  uint8_t placed = 0; int m = 0;
  bool done[NSVC] = { false };
  while (m < NSVC) {
    bool moved = false;
    for (int i = 0; i < NSVC; i++) {
      int k = list[i];
      if (done[k]) continue;
      if ((SV[k].req & ~placed) == 0) { out[m++] = k; placed |= (1 << k); done[k] = true; moved = true; break; }
    }
    if (!moved) {   // non succede (nessun ciclo), ma meglio non bloccarsi
      for (int i = 0; i < NSVC; i++) if (!done[list[i]]) { out[m++] = list[i]; done[list[i]] = true; }
    }
  }
}

static String orderCsv(const int* o) {
  String s;
  for (int i = 0; i < NSVC; i++) { if (i) s += ","; s += SV[o[i]].id; }
  return s;
}

void bootRun() {
  Preferences p;
  p.begin("vos", false);
  String saved = p.getString("border", "");
  g_try = p.getUChar("btry", 0);
  if (saved.length() && saved != DEF_ORDER) {
    if (g_try >= 2) {
      vlog("BOOT: l'ordine scelto non ha funzionato, torno a quello predefinito");
      p.remove("border"); p.putUChar("btry", 0);
      saved = ""; g_try = 0; g_fell = true;
    } else {
      g_custom = true;
      p.putUChar("btry", g_try + 1);
    }
  }
  p.end();
  sortOrder(saved, g_ord);
  uint32_t t00 = millis();
  for (int i = 0; i < NSVC; i++) {
    int k = g_ord[i];
    g_at[k] = millis() - t00;
    uint32_t t0 = millis();
    SV[k].start();
    g_ms[k] = millis() - t0;
  }
  vlog("BOOT: servizi avviati in %lu ms (%s)", (unsigned long)(millis() - t00), orderCsv(g_ord).c_str());
}

void bootStable() {
  if (g_stable || millis() < 60000UL) return;
  g_stable = true;
  if (g_custom || g_try) {
    Preferences p; p.begin("vos", false); p.putUChar("btry", 0); p.end();
    g_try = 0;
  }
}

bool bootSetOrder(const String& csv, String& result, String& err) {
  if (csv.length() > 80) { err = "Ordine non valido"; return false; }
  int o[NSVC];
  sortOrder(csv, o);
  result = orderCsv(o);
  Preferences p; p.begin("vos", false);
  if (result == DEF_ORDER) p.remove("border"); else p.putString("border", result);
  p.putUChar("btry", 0);
  p.end();
  vlog("BOOT: nuovo ordine di avvio %s", result.c_str());
  return true;
}

void bootResetOrder() {
  Preferences p; p.begin("vos", false); p.remove("border"); p.putUChar("btry", 0); p.end();
  vlog("BOOT: ordine di avvio predefinito");
}

String bootJson() {
  String j = "{\"order\":[";
  for (int i = 0; i < NSVC; i++) { if (i) j += ","; j += "\"" + String(SV[g_ord[i]].id) + "\""; }
  j += "],\"custom\":" + String(g_custom ? "true" : "false");
  j += ",\"fell\":" + String(g_fell ? "true" : "false");
  j += ",\"stable\":" + String(g_stable ? "true" : "false");
  j += ",\"svc\":[";
  for (int i = 0; i < NSVC; i++) {
    if (i) j += ",";
    j += "{\"id\":\"" + String(SV[i].id) + "\",\"req\":[";
    bool f = true;
    for (int b = 0; b < NSVC; b++) if (SV[i].req & (1 << b)) { if (!f) j += ","; f = false; j += "\"" + String(SV[b].id) + "\""; }
    j += "],\"parent\":" + (SV[i].parent == 255 ? String("null") : "\"" + String(SV[SV[i].parent].id) + "\"");
    j += ",\"at\":" + String((unsigned long)g_at[i]) + ",\"ms\":" + String((unsigned long)g_ms[i]) + "}";
  }
  j += "]}";
  return j;
}
