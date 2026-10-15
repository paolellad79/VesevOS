// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_app_rt.cpp (vedi vos_app_rt.h)
#include "vos_app_rt.h"
#if VOS_WITH_BERRY
#include <stdlib.h>
#include <string.h>
#include "../core/vos_i18n.h"
#include "../core/vos_util.h"

struct Slot { char k; char id[13]; long iv, mn, mx; char sv[RT_TXT + 1]; int32_t* ring; uint16_t pts, head, cnt; };   // k: s l c t b g oppure d (dato)
struct Ev { uint8_t slot; bool isNum; long iv; char sv[RT_TXT + 1]; };
static Slot g_slot[RT_SLOTS]; static int g_n = 0;
static Ev g_ev[RT_EVQ]; static int g_evHead = 0, g_evN = 0;
static char g_name[25] = ""; static bool g_loaded = false;
static SemaphoreHandle_t g_mx = NULL;

static void lock() { if (!g_mx) g_mx = xSemaphoreCreateMutex(); if (g_mx) xSemaphoreTake(g_mx, portMAX_DELAY); }
static void unlock() { if (g_mx) xSemaphoreGive(g_mx); }

static bool hex6(const char* s, char* out) {                    // "rrggbb" o "#rrggbb" -> minuscolo, 6 cifre
  if (*s == '#') s++;
  if (strlen(s) != 6) return false;
  for (int i = 0; i < 6; i++) { char c = s[i]; if (c >= 'A' && c <= 'F') c += 32; if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return false; out[i] = c; }
  out[6] = 0; return true;
}
static bool cleanTxt(const char* in, char* out) {               // massimo 40 caratteri, niente caratteri di controllo
  size_t n = strlen(in); if (n > RT_TXT) return false;
  for (size_t i = 0; i < n; i++) { unsigned char c = (unsigned char)in[i]; if (c < 32 || c == 127) return false; }
  memcpy(out, in, n + 1); return true;
}
static bool parseLong(const char* s, long& v) {
  if (!*s) return false; size_t i = (*s == '-') ? 1 : 0; if (!s[i] || strlen(s + i) > 7) return false;
  for (size_t k = i; s[k]; k++) if (s[k] < '0' || s[k] > '9') return false;
  v = atol(s); return true;
}
static void ringPush(Slot& s, long v) {
  if (!s.ring || s.pts == 0) return;
  if (v > 2000000000L) v = 2000000000L; if (v < -2000000000L) v = -2000000000L;
  s.ring[s.head] = (int32_t)v; s.head = (uint16_t)((s.head + 1) % s.pts); if (s.cnt < s.pts) s.cnt++; s.iv = v;
}

void rtFree() { lock(); for (int i = 0; i < g_n; i++) { if (g_slot[i].ring) { free(g_slot[i].ring); g_slot[i].ring = NULL; } } g_n = 0; g_loaded = false; g_evN = 0; unlock(); }

void rtInit(const AppInfo& a) {
  rtFree();
  lock();
  memset(g_slot, 0, sizeof(g_slot)); g_evHead = g_evN = 0;
  strncpy(g_name, a.name.c_str(), sizeof(g_name) - 1); g_name[sizeof(g_name) - 1] = 0;
  for (int i = 0; i < a.nctl && g_n < RT_SLOTS; i++) {
    Slot& s = g_slot[g_n++]; const AppCtl& c = a.ctl[i];
    s.k = c.k; strncpy(s.id, c.id, 12); s.id[12] = 0; s.mn = c.mn; s.mx = c.mx; s.pts = c.pts;
    if (c.k == 'l') s.iv = c.mn;
    if (c.k == 'c') strcpy(s.sv, "000000");
    if (c.k == 'g') { s.ring = (int32_t*)malloc(sizeof(int32_t) * c.pts); if (!s.ring) s.pts = 0; }
  }
  // [data]: chiave=valore. Se la chiave e un controllo, e il suo valore iniziale; altrimenti e un dato dell'app
  size_t pos = 0;
  while (pos < (size_t)a.data.length()) {
    int e = a.data.indexOf('\n', pos); if (e < 0) e = a.data.length();
    String ln = a.data.substring(pos, e); pos = e + 1; int q = ln.indexOf('='); if (q < 1) continue;
    String k = ln.substring(0, q), v = ln.substring(q + 1);
    int f = -1; for (int i = 0; i < g_n; i++) if (k == g_slot[i].id) f = i;
    if (f < 0) {
      if (g_n >= RT_SLOTS) continue;
      Slot& s = g_slot[g_n++]; s.k = 'd'; strncpy(s.id, k.c_str(), 12); s.id[12] = 0; strncpy(s.sv, v.c_str(), RT_TXT); s.sv[RT_TXT] = 0; continue;
    }
    Slot& s = g_slot[f]; long n;
    if (s.k == 's') s.iv = (v == "1" || v == "true") ? 1 : 0;
    else if (s.k == 'l') { if (parseLong(v.c_str(), n)) s.iv = n < s.mn ? s.mn : n > s.mx ? s.mx : n; }
    else if (s.k == 'c') { char h[8]; if (hex6(v.c_str(), h)) strcpy(s.sv, h); }
    else if (s.k == 't') { char t[RT_TXT + 1]; if (cleanTxt(v.c_str(), t)) strcpy(s.sv, t); }
    else if (s.k == 'g') { if (parseLong(v.c_str(), n)) ringPush(s, n); }
  }
  g_loaded = true;
  unlock();
}

bool rtLoaded() { return g_loaded; }
String rtName() { return String(g_name); }

static int find(const char* id) { for (int i = 0; i < g_n; i++) if (strcmp(g_slot[i].id, id) == 0) return i; return -1; }

bool rtGet(const char* id, bool& isNum, long& iv, char* sv, size_t n) {
  lock(); int i = find(id); bool ok = i >= 0;
  if (ok) {
    Slot& s = g_slot[i]; isNum = (s.k == 's' || s.k == 'l' || s.k == 'b' || s.k == 'g'); iv = s.iv;
    strncpy(sv, s.sv, n - 1); sv[n - 1] = 0;
  }
  unlock(); return ok;
}

bool rtSetApp(const char* id, bool isNum, long iv, const char* sv, const char** why) {
  static char numbuf[16];
  lock(); int i = find(id);
  if (i < 0) { unlock(); *why = "id sconosciuto"; return false; }
  Slot& s = g_slot[i]; bool ok = true; *why = "";
  switch (s.k) {
    case 's': { long v = isNum ? iv : (sv && (strcmp(sv, "1") == 0 || strcmp(sv, "true") == 0)); s.iv = v ? 1 : 0; break; }
    case 'b': s.iv = (isNum ? iv : 0) ? 1 : 0; break;
    case 'l': { long v = iv; if (!isNum) { if (!sv || !parseLong(sv, v)) { ok = false; *why = "serve un numero"; break; } } s.iv = v < s.mn ? s.mn : v > s.mx ? s.mx : v; break; }
    case 'g': { long v = iv; if (!isNum) { if (!sv || !parseLong(sv, v)) { ok = false; *why = "serve un numero"; break; } } ringPush(s, v); break; }
    case 'c': { char h[8]; if (isNum || !sv || !hex6(sv, h)) { ok = false; *why = "colore non valido (6 cifre esadecimali)"; } else strcpy(s.sv, h); break; }
    default: {                                                       // t, d: testo
      char t[RT_TXT + 1];
      if (isNum) { snprintf(numbuf, sizeof(numbuf), "%ld", iv); sv = numbuf; }
      if (!sv || !cleanTxt(sv, t)) { ok = false; *why = "testo troppo lungo (massimo 40) o con caratteri di controllo"; } else strcpy(s.sv, t);
    }
  }
  unlock(); return ok;
}

bool rtPop(char* id, bool& isNum, long& iv, char* sv, size_t n) {
  lock(); bool ok = g_evN > 0;
  if (ok) {
    Ev& e = g_ev[g_evHead]; strncpy(id, g_slot[e.slot].id, 12); id[12] = 0; isNum = e.isNum; iv = e.iv; strncpy(sv, e.sv, n - 1); sv[n - 1] = 0;
    g_evHead = (g_evHead + 1) % RT_EVQ; g_evN--;
  }
  unlock(); return ok;
}

bool rtSetWeb(const String& idS, const String& v, String& err) {
  lock();
  int i = idS.length() <= 12 ? find(idS.c_str()) : -1;
  if (i < 0 || g_slot[i].k == 'g' || g_slot[i].k == 'd') { unlock(); err = tr("Controllo non valido"); return false; }
  Slot& s = g_slot[i]; Ev e; memset(&e, 0, sizeof(e)); e.slot = (uint8_t)i;
  if (s.k == 's') { if (v == "1" || v == "true") e.iv = 1; else if (v == "0" || v == "false") e.iv = 0; else { unlock(); err = tr("Valore non valido"); return false; } e.isNum = true; }
  else if (s.k == 'b') { e.iv = 1; e.isNum = true; }
  else if (s.k == 'l') { long n; if (!parseLong(v.c_str(), n) || n < s.mn || n > s.mx) { unlock(); err = tr("Valore fuori limite"); return false; } e.iv = n; e.isNum = true; }
  else if (s.k == 'c') { char h[8]; if (!hex6(v.c_str(), h)) { unlock(); err = tr("Valore non valido"); return false; } strcpy(e.sv, h); }
  else { char t[RT_TXT + 1]; if (!cleanTxt(v.c_str(), t)) { unlock(); err = tr("Valore non valido"); return false; } strcpy(e.sv, t); }
  if (g_evN >= RT_EVQ) { unlock(); err = tr("App occupata, riprova"); return false; }
  if (s.k == 's' || s.k == 'l') s.iv = e.iv; else if (s.k == 'c' || s.k == 't') strcpy(s.sv, e.sv);
  g_ev[(g_evHead + g_evN) % RT_EVQ] = e; g_evN++;
  unlock(); return true;
}

String rtValuesJson() {
  String j = "[";
  lock();
  for (int i = 0; i < g_n; i++) {
    const Slot& s = g_slot[i]; if (s.k == 'd') continue;
    if (j.length() > 1) j += ",";
    j += String("{\"id\":\"") + s.id + "\",";
    if (s.k == 's' || s.k == 'l' || s.k == 'b') j += "\"n\":" + String(s.iv) + "}";
    else if (s.k == 'g') {
      j += "\"a\":[";
      for (int k = 0; k < s.cnt; k++) { if (k) j += ","; j += String((long)s.ring[(s.head + s.pts - s.cnt + k) % s.pts]); }
      j += "]}";
    } else j += "\"s\":\"" + jsonEscape(String(s.sv)) + "\"}";
  }
  unlock();
  return j + "]";
}
#endif
