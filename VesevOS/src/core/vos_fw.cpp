// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_fw.cpp
#include "vos_fw.h"
#include "vos_config.h"
#include "../net/vos_net.h"
#include "vos_util.h"
#include "vos_log.h"
#include "vos_i18n.h"
#include "../drivers/vos_drv_wifi.h"

#define TRY_MS (120UL * 1000UL)
#define REJ_N 8

static bool g_try = false;
static uint32_t g_tryUntil = 0;
static String g_backup;                       // regole di prima (testo della sezione), per tornare indietro
static uint8_t g_bMode; static FwRule g_bRules[VOS_FW_MAX]; static uint8_t g_bN;
struct Rej { uint32_t ip; uint32_t n; uint32_t lastLog; };
static Rej g_rej[REJ_N];


static bool inList(uint32_t ip) {
  for (int i = 0; i < cfg.fwN; i++) if (cfg.fw[i].on && ip >= cfg.fw[i].a && ip <= cfg.fw[i].b) return true;
  return false;
}

bool fwAllow(uint32_t ip, bool ntp) {
  if (cfg.fwMode == 0) return true;
  if (ntp && !cfg.fwNtp) return true;
  if ((ip & 0xFFFFFF00UL) == 0xC0A80400UL) return true;      // hotspot 192.168.4.x: sempre (protetto da WPA2 con password unica)
  if (ip == 0x7F000001UL) return true;
  switch (cfg.fwMode) {
    case 1: {                                                // solo la mia rete
      if (netState() != NET_CLIENT_OK) return true;
      uint32_t me = drvWifiLocalIp(), m = drvWifiMask();
      return m && ((ip & m) == (me & m));
    }
    case 2: return inList(ip);
    case 3: return !inList(ip);
  }
  return true;
}

void fwNoteRejected(uint32_t ip) {
  Rej* s = &g_rej[0];
  for (int i = 0; i < REJ_N; i++) { if (g_rej[i].ip == ip) { s = &g_rej[i]; break; } if (g_rej[i].n < s->n) s = &g_rej[i]; }
  if (s->ip != ip) { s->ip = ip; s->n = 0; s->lastLog = 0; }
  s->n++;
  if (!s->lastLog || millis() - s->lastLog > 60000UL) { s->lastLog = millis(); if (!s->lastLog) s->lastLog = 1; vlog("FIREWALL: rifiutato %s (%lu volte)", ipToStr(ip).c_str(), (unsigned long)s->n); }
}

bool fwTryStart(uint32_t keepIp) {
  (void)keepIp;
  if (!g_try) { g_bMode = cfg.fwMode; g_bN = cfg.fwN; for (int i = 0; i < VOS_FW_MAX; i++) g_bRules[i] = cfg.fw[i]; }
  g_try = true; g_tryUntil = millis() + TRY_MS; if (!g_tryUntil) g_tryUntil = 1;
  vlog("FIREWALL: regola nuova in prova per 2 minuti");
  return true;
}

void fwConfirm() {
  if (!g_try) return;
  g_try = false;
  vlog("FIREWALL: regola confermata");
}

bool fwTrying() { return g_try; }
uint32_t fwTryLeft() { if (!g_try) return 0; int32_t l = (int32_t)(g_tryUntil - millis()); return l > 0 ? (uint32_t)l / 1000 + 1 : 0; }

void fwTick() {
  if (g_try && (int32_t)(g_tryUntil - millis()) <= 0) {
    g_try = false;
    cfg.fwMode = g_bMode; cfg.fwN = g_bN; for (int i = 0; i < VOS_FW_MAX; i++) cfg.fw[i] = g_bRules[i];
    cfgSetOrigin("filtro IP: prova scaduta"); cfgSave();
    vlog("FIREWALL: prova non confermata, torna la regola di prima");
  }
}

void fwOff(const char* why) {
  g_try = false;
  if (cfg.fwMode == 0) return;
  cfg.fwMode = 0; cfgSetOrigin(why); cfgSave();
  vlog("FIREWALL: spento (%s)", why);
}

String fwJson(uint32_t you) {
  String j = "{\"mode\":" + String(cfg.fwMode) + ",\"ntp\":" + String(cfg.fwNtp ? "true" : "false") + ",\"try\":" + String(fwTryLeft()) +
             ",\"you\":\"" + ipToStr(you) + "\",\"youOk\":" + String(fwAllow(you, false) ? "true" : "false") + ",\"rules\":[";
  for (int i = 0; i < cfg.fwN; i++) {
    if (i) j += ",";
    j += "{\"from\":\"" + ipToStr(cfg.fw[i].a) + "\",\"to\":\"" + ipToStr(cfg.fw[i].b) + "\",\"name\":\"" + jsonEscape(cfg.fw[i].name) + "\",\"on\":" + String(cfg.fw[i].on ? "true" : "false") + "}";
  }
  j += "],\"rejected\":[";
  bool first = true;
  for (int i = 0; i < REJ_N; i++) {
    if (!g_rej[i].ip) continue;
    if (!first) j += ","; first = false;
    j += "{\"ip\":\"" + ipToStr(g_rej[i].ip) + "\",\"n\":" + String((unsigned long)g_rej[i].n) + "}";
  }
  return j + "]}";
}

String fwText() {
  static const char* const M[] = {"spento", "solo la mia rete", "lista consentita", "lista bloccati"};
  String t = trf("Filtro IP: %s", tr(M[cfg.fwMode & 3])) + "\n";
  for (int i = 0; i < cfg.fwN; i++)
    t += String(i + 1) + "  " + (cfg.fw[i].a == cfg.fw[i].b ? ipToStr(cfg.fw[i].a) : ipToStr(cfg.fw[i].a) + "-" + ipToStr(cfg.fw[i].b)) + (cfg.fw[i].on ? "  " : "  (off)  ") + cfg.fw[i].name + "\n";
  if (g_try) t += trf("Regola in prova: %lu s rimasti (firewall confirm)", (unsigned long)fwTryLeft()) + "\n";
  return t;
}
