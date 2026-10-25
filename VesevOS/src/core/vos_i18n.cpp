// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_i18n.cpp
#include "vos_i18n.h"
#include "vos_config.h"
#include "vos_util.h"
#include "vos_log.h"
#include "../drivers/vos_drv_fs.h"
#include "../drivers/vos_drv_heap.h"
#include "vos_lang_it.h"
#include <stdarg.h>
#include <stdio.h>

#define LANG_DIR      "/lang"
#define LANG_MAX_SIZE 262144        // i file it/en/es/de pesano circa 185 KB (fino alla 1.7.58 il limite era 64 KB: es e de venivano rifiutati); finisce in PSRAM

struct LangEntry { const char* k; const char* v; };
struct LangDict { char* buf; LangEntry* ent; int n; char code[LANG_CODE_BUF]; };
static LangDict* g_d = nullptr;      // dizionario in uso (nullptr = solo italiano)
static LangDict* g_old = nullptr;    // quello appena sostituito: resta valido finche non ne arriva un altro,
                                     // cosi chi sta ancora leggendo un testo non trova memoria liberata

static void* bigAlloc(size_t n) {
  return drvMallocBig(n);
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

bool langCodeValid(std::string_view c) {
  if (c.size() < 2 || c.size() > 8) return false;
  for (char ch : c) {
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
static bool loadFile(const char* code, ErrBuf& err) {
  char* buf = nullptr; size_t got = 0;
  {                                                  // dalla 1.7.59 anche l'inglese e un file (prima era dentro il firmware: 59 KB)
    char path[sizeof(LANG_DIR) + LANG_CODE_BUF + 6];
    snprintf(path, sizeof path, LANG_DIR "/%s.json", code);
    VosFile f = drvFsOpen(path, "r");
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
  d->buf = buf; d->ent = ent; d->n = n; snprintf(d->code, sizeof d->code, "%s", code);
  swapDict(d);
  vlog("LANG: %s (%d voci)", code, n);
  return true;
}

void langInit() {
  if (cfg.lang == "it" || !langCodeValid(cfg.lang.view())) { swapDict(nullptr); return; }
  ErrBuf err;
  if (!loadFile(cfg.lang.c_str(), err)) vlog("LANG: %s non caricata, uso l'italiano", cfg.lang.c_str());
}

bool langSet(const char* code, ErrBuf& err) {
  if (!langCodeValid(code)) { err = tr("Lingua non valida"); return false; }
  if (strcmp(code, "it") == 0) { swapDict(nullptr); cfg.lang = "it"; cfgSave(); return true; }
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

TrText trf(const char* it, ...) {
  const char* f = tr(it);
  TrText r;
  va_list ap; va_start(ap, it);
  int n = vsnprintf(r.small_, sizeof(r.small_), f, ap);
  va_end(ap);
  if (n < (int)sizeof(r.small_)) return r;           // entra nel piccolo (o errore di formato: resta il testo troncato)
  r.big_ = (char*)malloc(n + 1);
  if (!r.big_) return r;                             // manca la memoria: resta il testo troncato
  va_start(ap, it);
  vsnprintf(r.big_, n + 1, f, ap);
  va_end(ap);
  return r;
}

// ---------- intestazione dei file di lingua (nome, codice locale, bandiera) ----------

// Testi letti dall'inizio di un file di lingua. Un solo blocco di heap per tutto (la bandiera pesa fino a 1 KB).
struct LangHead {
  char raw[2049];
  char name[LANG_NAME_BUF * 2];
  char loc[16];
  char flag[1025];
};

// Valore di una chiave speciale ("_name", "_locale", "_flag") cercata nell'inizio del testo; scritto in out (tagliato se non ci sta).
// Ritorna false se la chiave manca o il valore non ci sta tutto.
static bool headKey(const char* txt, const char* key, char* out, size_t cap) {
  out[0] = 0;
  const char* p = strstr(txt, key);
  if (!p) return false;
  p += strlen(key);
  while (*p && *p != ':') p++;
  while (*p && *p != '"') p++;
  if (*p != '"') return false;
  p++;
  size_t n = 0;
  bool whole = true;
  while (*p && *p != '"') {
    if (*p == '\\' && p[1]) { p++; if (*p == 'u' && p[1] && p[2] && p[3] && p[4]) { p += 5; continue; } }   // la bandiera non usa caratteri speciali
    if (n + 1 < cap) out[n++] = *p; else whole = false;
    p++;
  }
  out[n] = 0;
  return whole;
}

static void fileHead(const char* code, LangHead& h) {
  snprintf(h.name, sizeof h.name, "%s", code);
  h.loc[0] = 0; h.flag[0] = 0;
  char path[sizeof(LANG_DIR) + LANG_CODE_BUF + 6];
  snprintf(path, sizeof path, LANG_DIR "/%s.json", code);
  VosFile f = drvFsOpen(path, "r");
  if (!f) return;
  size_t n = f.read((uint8_t*)h.raw, sizeof(h.raw) - 1);
  f.close();
  h.raw[n] = 0;
  char nm[sizeof h.name];
  if (headKey(h.raw, "\"_name\"", nm, sizeof nm) && nm[0]) snprintf(h.name, sizeof h.name, "%s", nm);
  headKey(h.raw, "\"_locale\"", h.loc, sizeof h.loc);
  bool whole = headKey(h.raw, "\"_flag\"", h.flag, sizeof h.flag);
  if (!whole || strncmp(h.flag, "<svg", 4) != 0) h.flag[0] = 0;   // la pagina la mostra come immagine (<img>): niente script
}

static bool itemJson(HeapBuf& out, const char* code, const char* name, const char* loc, const char* flag) {
  return out.add("{\"code\":\"") && out.add(code) && out.add("\",\"name\":\"") && out.addJson(name) &&
         out.add("\",\"loc\":\"") && out.addJson(loc) && out.add("\",\"flag\":\"") && out.addJson(flag) && out.add("\"}");
}

// Prossimo file /lang/<codice>.json con codice valido e diverso da "it": scrive il codice in code. false = finiti.
static bool nextLangCode(VosFile& d, char* code) {
  for (VosFile e = d.openNextFile(); e; e = d.openNextFile()) {
    if (e.isDirectory()) continue;
    const char* n = e.name();
    const char* sl = strrchr(n, '/'); if (sl) n = sl + 1;
    size_t len = strlen(n);
    if (len < 6 || strcmp(n + len - 5, ".json") != 0) continue;
    std::string_view c(n, len - 5);
    if (c == "it" || !langCodeValid(c)) continue;
    snprintf(code, LANG_CODE_BUF, "%.*s", (int)c.size(), c.data());
    return true;
  }
  return false;
}

bool langListJson(HeapBuf& out) {
  bool ok = out.add('[') && itemJson(out, "it", "Italiano", "it-IT", LANG_FLAG_IT);
  VosFile d = drvFsOpen(LANG_DIR);
  if (ok && d && d.isDirectory()) {
    LangHead* h = (LangHead*)malloc(sizeof(LangHead));
    if (!h) { d.close(); return false; }
    char code[LANG_CODE_BUF];
    int n = 1;                                                          // "it" e gia nell'elenco
    while (ok && n < LANG_LIST_MAX && nextLangCode(d, code)) {
      n++;
      fileHead(code, *h);
      ok = out.add(',') && itemJson(out, code, h->name, h->loc, h->flag);
    }
    free(h);
  }
  if (d) d.close();
  return ok && out.add(']');
}

int langList(char codes[][LANG_CODE_BUF], char names[][LANG_NAME_BUF], int max) {
  int n = 0;
  if (n < max) { snprintf(codes[n], LANG_CODE_BUF, "it"); snprintf(names[n], LANG_NAME_BUF, "Italiano"); n++; }
  VosFile d = drvFsOpen(LANG_DIR);
  if (d && d.isDirectory()) {
    LangHead* h = (LangHead*)malloc(sizeof(LangHead));
    char code[LANG_CODE_BUF];
    while (h && n < max && nextLangCode(d, code)) {
      fileHead(code, *h);
      snprintf(codes[n], LANG_CODE_BUF, "%s", code); snprintf(names[n], LANG_NAME_BUF, "%s", h->name); n++;
    }
    free(h);
  }
  if (d) d.close();
  return n;
}
