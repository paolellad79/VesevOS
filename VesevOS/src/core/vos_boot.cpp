// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_boot.cpp
// I servizi "mobili" partono nell'ordine scelto dall'utente. Ogni servizio dichiara cosa richiede:
// l'ordine finale e sempre corretto in modo che ognuno parta DOPO quelli che richiede.
// I servizi fissi (log, pin, file/config, lingua) partono sempre per primi, in setup().
#include "vos_boot.h"
#include "vos_diario.h"
#include "vos_util.h"
#include "vos_parse.h"
#include "vos_strbuf.h"
#include "vos_log.h"
#include "vos_sys.h"
#include "vos_time.h"
#include "vos_hooks.h"
#include "vos_wd.h"
#include "../drivers/vos_drv_nvs.h"
#include "../drivers/vos_drv_time.h"

#define NSVC 9

struct Svc {
  const char* id;
  void (*start)();
  uint16_t req;      // maschera dei servizi richiesti
  uint8_t parent;    // genitore nella vista ad albero (255 = nessuno)
};

// pagina, regole, MQTT e ESP-NOW stanno sopra il nucleo: si avviano attraverso i ganci (vos_hooks.h)
static void webInitH()   { hookCall(g_hooks.webInit); }
static void rulesInitH() { hookCall(g_hooks.rulesInit); }
static void mqttInitH()  { hookCall(g_hooks.mqttInit); }
static void meshInitH()  { hookCall(g_hooks.meshInit); }
static void ledInitH()   { hookCall(g_hooks.ledInit); }
static void netInitH()   { hookCall(g_hooks.netInit); }

enum { S_SYS = 0, S_LED, S_NET, S_TIME, S_WEB, S_RULES, S_MQTT, S_MESH, S_WD };

static const Svc SV[NSVC] = {
  { "sys",   sysInit,   0,                                  255 },
  { "led",   ledInitH,  0,                                  255 },
  { "net",   netInitH,  0,                                  255 },
  { "time",  timeInit,  (uint16_t)(1 << S_NET),              S_NET },
  { "web",   webInitH,   (uint16_t)((1 << S_NET) | (1 << S_SYS)), S_NET },
  { "rules", rulesInitH, (uint16_t)((1 << S_TIME) | (1 << S_SYS)), S_TIME },
  { "mqtt",  mqttInitH, (uint16_t)((1 << S_NET) | (1 << S_SYS)), S_NET },
  { "mesh",  meshInitH, (uint16_t)((1 << S_NET) | (1 << S_SYS)), S_NET },
  { "wd",    wdInit,    (uint16_t)(1 << S_SYS),                 S_SYS },
};

static const char* DEF_ORDER = "sys,led,net,time,web,rules,mqtt,mesh,wd";

static int   g_ord[NSVC];
static uint32_t g_at[NSVC], g_ms[NSVC];
static bool  g_custom = false, g_fell = false, g_stable = false;
static uint8_t g_try = 0;

static int svcIndex(std::string_view id) {
  for (int i = 0; i < NSVC; i++) if (id == SV[i].id) return i;
  return -1;
}

// csv -> ordine finale (indici). Nomi sconosciuti o doppi si scartano, i mancanti si aggiungono.
static void sortOrder(std::string_view csv, int* out) {
  int list[NSVC], n = 0;
  uint16_t seen = 0;
  size_t p = 0;
  while (p <= csv.size() && n < NSVC) {
    size_t e = csv.find(',', p);
    if (e == std::string_view::npos) e = csv.size();
    int k = svcIndex(trimView(csv.substr(p, e - p)));
    if (k >= 0 && !(seen & (1 << k))) { list[n++] = k; seen |= (1 << k); }
    p = e + 1;
  }
  for (int i = 0; i < NSVC; i++) if (!(seen & (1 << i))) list[n++] = i;
  // ordinamento stabile: ogni servizio dopo quelli che richiede
  uint16_t placed = 0; int m = 0;
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

#define ORDER_BUF 96   // "sys,led,net,time,web,rules,mqtt,mesh,wd" sono 40 caratteri: l'ordine intero ci sta largo

static void orderCsv(const int* o, char* out, size_t cap) {
  StrBuf s(out, cap);
  for (int i = 0; i < NSVC; i++) { if (i) s.add(','); s.add(SV[o[i]].id); }
}

void bootRun() {
  DrvNvs p;
  p.begin("vos", false);
  char saved[ORDER_BUF] = "";
  p.getString("border", saved, sizeof saved);                   // se manca o non ci sta: resta vuoto
  g_try = p.getUChar("btry", 0);
  if (saved[0] && strcmp(saved, DEF_ORDER) != 0) {
    if (g_try >= 2) {
      vlog("BOOT: l'ordine scelto non ha funzionato, torno a quello predefinito");
      p.remove("border"); p.putUChar("btry", 0);
      saved[0] = 0; g_try = 0; g_fell = true;
    } else {
      g_custom = true;
      p.putUChar("btry", g_try + 1);
    }
  }
  p.end();
  sortOrder(saved, g_ord);
  uint32_t t00 = drvMillis();
  for (int i = 0; i < NSVC; i++) {
    int k = g_ord[i];
    g_at[k] = drvMillis() - t00;
    uint32_t t0 = drvMillis();
    diaryStage(SV[k].id);
    SV[k].start();
    g_ms[k] = drvMillis() - t0;
  }
  char ord[ORDER_BUF]; orderCsv(g_ord, ord, sizeof ord);
  vlog("BOOT: servizi avviati in %lu ms (%s)", (unsigned long)(drvMillis() - t00), ord);
}

void bootStable() {
  if (g_stable || drvMillis() < 60000UL) return;
  g_stable = true;
  if (g_custom || g_try) {
    DrvNvs p; p.begin("vos", false); p.putUChar("btry", 0); p.end();
    g_try = 0;
  }
}

bool bootSetOrder(const char* csv, char* result, size_t resultLen, ErrBuf& err) {
  if (strlen(csv) > 80) { err = "Ordine non valido"; return false; }
  int o[NSVC];
  sortOrder(csv, o);
  char order[ORDER_BUF]; orderCsv(o, order, sizeof order);
  snprintf(result, resultLen, "%s", order);
  DrvNvs p; p.begin("vos", false);
  if (strcmp(order, DEF_ORDER) == 0) p.remove("border"); else p.putString("border", order);
  p.putUChar("btry", 0);
  p.end();
  vlog("BOOT: nuovo ordine di avvio %s", order);
  return true;
}

void bootResetOrder() {
  DrvNvs p; p.begin("vos", false); p.remove("border"); p.putUChar("btry", 0); p.end();
  vlog("BOOT: ordine di avvio predefinito");
}

bool bootJson(HeapBuf& j) {
  bool ok = j.add("{\"order\":[");
  for (int i = 0; ok && i < NSVC; i++) ok = (!i || j.add(',')) && j.addf("\"%s\"", SV[g_ord[i]].id);
  ok = ok && j.addf("],\"custom\":%s,\"fell\":%s,\"stable\":%s,\"svc\":[", g_custom ? "true" : "false", g_fell ? "true" : "false", g_stable ? "true" : "false");
  for (int i = 0; ok && i < NSVC; i++) {
    ok = (!i || j.add(',')) && j.addf("{\"id\":\"%s\",\"req\":[", SV[i].id);
    bool first = true;
    for (int b = 0; ok && b < NSVC; b++) {
      if (!(SV[i].req & (1 << b))) continue;
      ok = (first || j.add(',')) && j.addf("\"%s\"", SV[b].id);
      first = false;
    }
    ok = ok && j.add("],\"parent\":") && (SV[i].parent == 255 ? j.add("null") : j.addf("\"%s\"", SV[SV[i].parent].id)) &&
         j.addf(",\"at\":%lu,\"ms\":%lu}", (unsigned long)g_at[i], (unsigned long)g_ms[i]);
  }
  return ok && j.add("]}");
}
