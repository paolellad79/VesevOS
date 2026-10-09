// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_i18n.cpp
#include "vos_i18n.h"
#include "vos_config.h"
#include "vos_util.h"
#include "vos_log.h"
#include "../drivers/vos_drv_fs.h"
#include "vos_lang_en.h"
#include "vos_inflate.h"
#include <stdarg.h>
#include <stdio.h>

#define LANG_DIR      "/lang"
#define LANG_MAX_SIZE 65536

struct LangEntry { const char* k; const char* v; };
struct LangDict { char* buf; LangEntry* ent; int n; String code; };
static LangDict* g_d = nullptr;      // dizionario in uso (nullptr = solo italiano)
static LangDict* g_old = nullptr;    // quello appena sostituito: resta valido finche non ne arriva un altro,
                                     // cosi chi sta ancora leggendo un testo non trova memoria liberata

static void* bigAlloc(size_t n) {
  void* p = psramFound() ? ps_malloc(n) : nullptr;
  if (!p) p = malloc(n);
  return p;
}

static void freeDict(LangDict* d) {
  if (!d) return;
  free(d->ent); free(d->buf);
  delete d;
}

// Mette in uso un nuovo dizionario (anche nullptr = italiano) e ritira il precedente.
static void swapDict(LangDict* nd) {
  LangDict* old = g_d;
  g_d = nd;                          // scambio con una sola assegnazione
  freeDict(g_old);
  g_old = old;
}

bool langCodeValid(const String& c) {
  if (c.length() < 2 || c.length() > 8) return false;
  for (size_t i = 0; i < c.length(); i++) {
    char ch = c[i];
    if (!((ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9'))) return false;
  }
  return true;
}

static int hexv(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}

// Legge una stringa JSON da p (dopo l'apice iniziale), la decodifica sul posto e termina con 0.
// Ritorna il puntatore alla stringa; p avanza oltre l'apice finale. nullptr se non chiusa.
static char* readStr(char*& p) {
  char* start = p; char* w = p;
  while (*p) {
    char c = *p++;
    if (c == '"') { *w = 0; return start; }
    if (c != '\\') { *w++ = c; continue; }
    char e = *p++;
    if (!e) return nullptr;
    switch (e) {
      case 'n': *w++ = '\n'; break;
      case 't': *w++ = '\t'; break;
      case 'r': break;                       // il ritorno a capo si scarta
      case 'u': {
        int h[4];
        for (int i = 0; i < 4; i++) { h[i] = hexv(p[i]); if (h[i] < 0) return nullptr; }
        p += 4;
        unsigned cp = (h[0] << 12) | (h[1] << 8) | (h[2] << 4) | h[3];
        if (cp < 0x80) *w++ = (char)cp;
        else if (cp < 0x800) { *w++ = (char)(0xC0 | (cp >> 6)); *w++ = (char)(0x80 | (cp & 0x3F)); }
        else { *w++ = (char)(0xE0 | (cp >> 12)); *w++ = (char)(0x80 | ((cp >> 6) & 0x3F)); *w++ = (char)(0x80 | (cp & 0x3F)); }
        break;
      }
      default: *w++ = e; break;              // \" \\ \/
    }
  }
  return nullptr;
}

// Carica /lang/<code>.json. Ritorna true se ha almeno una voce.
static bool loadFile(const String& code, String& err) {
  char* buf = nullptr; size_t got = 0;
  if (code == "en") {                                // inglese: dentro il firmware
    size_t raw = gzipRawSize(LANG_EN_GZ, LANG_EN_GZ_LEN);
    buf = raw ? (char*)bigAlloc(raw + 1) : nullptr;
    if (!buf) { err = tr("Memoria esaurita"); return false; }
    got = gzipUnpack(LANG_EN_GZ, LANG_EN_GZ_LEN, (uint8_t*)buf, raw);
    if (got != raw) { free(buf); err = tr("Lingua non installata"); return false; }
    buf[got] = 0;
  } else {
    String path = String(LANG_DIR) + "/" + code + ".json";
    File f = drvFsOpen(path, "r");
    if (!f || f.isDirectory()) { err = tr("Lingua non installata"); return false; }
    size_t sz = f.size();
    if (sz < 2 || sz > LANG_MAX_SIZE) { f.close(); err = tr("File di lingua non valido"); return false; }
    buf = (char*)bigAlloc(sz + 1);
    if (!buf) { f.close(); err = tr("Memoria esaurita"); return false; }
    got = f.read((uint8_t*)buf, sz);
    f.close();
    buf[got] = 0;
  }
  // numero massimo di voci: ogni voce ha almeno 4 apici
  int cap = 1;
  for (size_t i = 0; i < got; i++) if (buf[i] == '"') cap++;
  cap = cap / 4 + 1;
  LangEntry* ent = (LangEntry*)bigAlloc(sizeof(LangEntry) * cap);
  if (!ent) { free(buf); err = tr("Memoria esaurita"); return false; }
  int n = 0;
  char* p = buf;
  while (*p && *p != '{') p++;
  if (*p == '{') p++;
  while (*p) {
    while (*p && *p != '"' && *p != '}') p++;
    if (*p != '"') break;
    p++;
    char* k = readStr(p);
    if (!k) break;
    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') p++;
    if (*p != ':') break;
    p++;
    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') p++;
    if (*p != '"') break;
    p++;
    char* v = readStr(p);
    if (!v) break;
    if (k[0] != '_' && n < cap) { ent[n].k = k; ent[n].v = v; n++; }
  }
  if (n == 0) { free(buf); free(ent); err = tr("File di lingua non valido"); return false; }
  LangDict* d = new LangDict();
  d->buf = buf; d->ent = ent; d->n = n; d->code = code;
  swapDict(d);
  vlog("LANG: %s (%d voci)", code.c_str(), n);
  return true;
}

void langInit() {
  if (cfg.lang == "it" || !langCodeValid(cfg.lang)) { swapDict(nullptr); return; }
  String err;
  if (!loadFile(cfg.lang, err)) vlog("LANG: %s non caricata, uso l'italiano", cfg.lang.c_str());
}

bool langSet(const String& code, String& err) {
  if (!langCodeValid(code)) { err = tr("Lingua non valida"); return false; }
  if (code == "it") { swapDict(nullptr); cfg.lang = "it"; cfgSave(); return true; }
  if (!loadFile(code, err)) return false;
  cfg.lang = code; cfgSave();
  return true;
}

const char* tr(const char* it) {
  LangDict* d = g_d;
  if (!d || !it) return it;
  for (int i = 0; i < d->n; i++) {
    if (strcmp(d->ent[i].k, it) == 0) return d->ent[i].v[0] ? d->ent[i].v : it;
  }
  return it;
}

String trf(const char* it, ...) {
  const char* f = tr(it);
  char buf[256];
  va_list ap; va_start(ap, it);
  int n = vsnprintf(buf, sizeof(buf), f, ap);
  va_end(ap);
  if (n < 0) return String(f);
  if (n < (int)sizeof(buf)) return String(buf);
  char* big = (char*)malloc(n + 1);
  if (!big) return String(buf);
  va_start(ap, it);
  vsnprintf(big, n + 1, f, ap);
  va_end(ap);
  String r(big); free(big);
  return r;
}

bool langBuiltin(const String& c) { return c == "it" || c == "en"; }

const uint8_t* langBuiltinJson(const String& code, size_t& len) {      // compresso (gzip): la pagina lo riceve cosi
  if (code == "en") { len = LANG_EN_GZ_LEN; return LANG_EN_GZ; }
  len = 0; return nullptr;
}

// Valore di una chiave speciale ("_name", "_locale", "_flag") cercata nell'inizio del testo
static String headKey(const char* txt, const char* key) {
  const char* p = strstr(txt, key);
  if (!p) return "";
  p += strlen(key);
  while (*p && *p != ':') p++;
  while (*p && *p != '"') p++;
  if (*p != '"') return "";
  p++;
  String out;
  while (*p && *p != '"') {
    if (*p == '\\' && p[1]) { p++; if (*p == 'u' && p[1] && p[2] && p[3] && p[4]) { p += 5; continue; } }   // la bandiera non usa caratteri speciali
    out += *p++;
  }
  return out;
}

static void fileHead(const String& code, String& name, String& loc, String& flag) {
  name = code; loc = ""; flag = "";
  File f = drvFsOpen(String(LANG_DIR) + "/" + code + ".json", "r");
  if (!f) return;
  char* b = (char*)malloc(2049);
  if (!b) { f.close(); return; }
  size_t n = f.read((uint8_t*)b, 2048);
  f.close();
  b[n] = 0;
  String nm = headKey(b, "\"_name\"");
  if (nm.length()) name = nm;
  loc = headKey(b, "\"_locale\"");
  flag = headKey(b, "\"_flag\"");
  free(b);
  if (flag.length() > 1024 || !flag.startsWith("<svg")) flag = "";   // la pagina la mostra come immagine (<img>): niente script
}

static String item(const String& code, const String& name, const String& loc, const String& flag) {
  return "{\"code\":\"" + code + "\",\"name\":\"" + jsonEscape(name) + "\",\"loc\":\"" + jsonEscape(loc) + "\",\"flag\":\"" + jsonEscape(flag) + "\"}";
}

String langListJson() {
  String j = "[" + item("it", "Italiano", "it-IT", LANG_FLAG_IT) + "," + item("en", LANG_EN_NAME, LANG_EN_LOCALE, LANG_EN_FLAG);
  File d = drvFsOpen(LANG_DIR);
  if (d && d.isDirectory()) {
    for (File e = d.openNextFile(); e; e = d.openNextFile()) {
      if (e.isDirectory()) continue;
      String n = e.name();
      int sl = n.lastIndexOf('/'); if (sl >= 0) n = n.substring(sl + 1);
      if (!n.endsWith(".json")) continue;
      String code = n.substring(0, n.length() - 5);
      if (langBuiltin(code) || !langCodeValid(code)) continue;
      String nm, loc, fl; fileHead(code, nm, loc, fl);
      j += "," + item(code, nm, loc, fl);
    }
    d.close();
  }
  j += "]";
  return j;
}

int langList(String* codes, String* names, int max) {
  int n = 0;
  if (n < max) { codes[n] = "it"; names[n] = "Italiano"; n++; }
  if (n < max) { codes[n] = "en"; names[n] = LANG_EN_NAME; n++; }
  File d = drvFsOpen(LANG_DIR);
  if (d && d.isDirectory()) {
    for (File e = d.openNextFile(); e && n < max; e = d.openNextFile()) {
      if (e.isDirectory()) continue;
      String fn = e.name();
      int sl = fn.lastIndexOf('/'); if (sl >= 0) fn = fn.substring(sl + 1);
      if (!fn.endsWith(".json")) continue;
      String code = fn.substring(0, fn.length() - 5);
      if (langBuiltin(code) || !langCodeValid(code)) continue;
      String nm, loc, fl; fileHead(code, nm, loc, fl);
      codes[n] = code; names[n] = nm; n++;
    }
    d.close();
  }
  return n;
}
