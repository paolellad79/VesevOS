// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_mod_files.cpp (vedi vos_mod.h): elenco dei file /devs/*.vdb (e dei vecchi *.vdp), JSON della pagina, testo della shell, installazione, salvataggio, eliminazione.
// Diviso da vos_mod.cpp per il limite di 500 righe, senza cambiare il comportamento.
#include "vos_mod.h"
#include <strings.h>
#include "vos_io.h"
#include "vos_sd.h"
#include "vos_mod_line.h"
#include "../core/vos_i18n.h"
#include "../core/vos_log.h"
#include "../core/vos_util.h"
#include "../core/vos_files.h"
#include "../core/vos_ext.h"
#include "../core/vos_vdesc.h"

// ---------- elenco dei file /devs/*.vdb (e dei vecchi *.vdp) ----------
// Se il nome finisce con ext, ne mette in stem la parte prima e ritorna la lunghezza; altrimenti 0.
static size_t stemOf(std::string_view name, const char* ext) {
  size_t e = strlen(ext);
  return name.size() > e && name.substr(name.size() - e) == ext ? name.size() - e : 0;
}
// Scrive in names (mx nomi da MOD_NAME_BUF byte) i primi nomi delle schede e ritorna quante ce ne sono in tutto. Crea la cartella se manca.
// Un nome con .vdb e .vdp insieme conta una volta sola.
int modScan(char (*names)[MOD_NAME_BUF], int mx) {
  HeapBuf j(FS_JSON_MAX);
  if (!fsListBuild(MOD_DIR, j)) return 0;                          // manca la memoria
  if (!strstr(j.c_str(), "\"ok\":true")) { ErrBuf e; fsMkdir(MOD_DIR, e); j.clear(); if (!fsListBuild(MOD_DIR, j)) return 0; }
  std::string_view v(j.c_str(), j.length());
  const size_t none = std::string_view::npos;
  int n = 0; size_t pos = 0;
  while (true) {
    size_t a = v.find("{\"n\":\"", pos); if (a == none) break;
    size_t b = v.find('"', a + 6); if (b == none) break;
    std::string_view name = v.substr(a + 6, b - (a + 6));
    size_t d = v.find("\"d\":", b);
    bool isDir = d != none && v.substr(d + 4, 4) == "true";
    pos = b + 1;
    size_t len = isDir ? 0 : stemOf(name, MOD_EXT);
    if (!len) len = isDir ? 0 : stemOf(name, MOD_EXT_OLD);
    if (!len) continue;
    size_t k = len < MOD_NAME_BUF - 1 ? len : MOD_NAME_BUF - 1;    // un nome troppo lungo si taglia: resta comunque non valido
    bool twin = false;
    for (int i = 0; i < n && i < mx && !twin; i++) twin = strlen(names[i]) == k && !memcmp(names[i], name.data(), k);
    if (twin) continue;
    if (n < mx) { memcpy(names[n], name.data(), k); names[n][k] = 0; }
    n++;
  }
  return n;
}
// Dice quali file del driver esistono in /devs.
static void whichFiles(const char* name, bool& hasNew, bool& hasOld) {
  hasNew = hasOld = false;
  HeapBuf j(FS_JSON_MAX);
  if (!fsListBuild(MOD_DIR, j)) return;
  char key[64];
  snprintf(key, sizeof key, "{\"n\":\"%s%s\"", name, MOD_EXT);     hasNew = strstr(j.c_str(), key) != nullptr;
  snprintf(key, sizeof key, "{\"n\":\"%s%s\"", name, MOD_EXT_OLD); hasOld = strstr(j.c_str(), key) != nullptr;
}
// Un nome vale per un solo driver: se in /devs c'e gia un driver .vde con questo nome, il .vdb non si salva.
static bool hasVde(const char* name) {
  HeapBuf j(FS_JSON_MAX);
  if (!fsListBuild(MOD_DIR, j)) return false;
  char key[64];
  snprintf(key, sizeof key, "{\"n\":\"%s%s\"", name, VOS_EXT_DRV_E);
  return strstr(j.c_str(), key) != nullptr;
}
void modFilePath(char* out, size_t cap, const char* name) {
  bool hasNew, hasOld; whichFiles(name, hasNew, hasOld);
  snprintf(out, cap, "%s/%s%s", MOD_DIR, name, hasOld && !hasNew ? MOD_EXT_OLD : MOD_EXT);
}

static bool load(const char* name, ModInfo& m, ErrBuf& err) {
  HeapBuf text(fsTextMax()); ErrBuf e;
  if (!utilNameOk(name, strlen(name))) { err = tr("Nome non valido"); return false; }
  char path[64];
  modFilePath(path, sizeof path, name);
  if (!fsReadText(path, text, e)) { err = e.empty() ? tr("File non trovato") : e.c_str(); return false; }
  return modParse(std::string_view(text.c_str(), text.length()), name, m, err);
}

static bool full(const HeapBuf& o) { return o.oom() || o.over(); }
static void addJ(HeapBuf& o, const char* s) { o.addJson(s, strlen(s)); }        // testo libero dentro virgolette JSON

// Campi comuni dell'elenco e del dettaglio (senza graffe)
static void headJson(const ModInfo& m, HeapBuf& o) {
  heapAddf(o, "\"n\":\"%s\",\"t\":\"", m.name); addJ(o, m.title);
  heapAddf(o, "\",\"c\":\"%s\",\"p\":\"%s\",\"ns\":%d,\"low\":%d,\"pull\":\"%c\",\"v\":\"%s\",\"a\":\"", m.cls, m.power, m.nsig, m.low ? 1 : 0, m.pull, m.ver);
  addJ(o, m.author); o.add("\",\"l\":\""); addJ(o, m.lic); o.add("\",\"w\":\""); addJ(o, m.warn); o.add("\",\"sig\":[");
  for (int i = 0; i < m.nsig; i++) heapAddf(o, "%s\"%s\"", i ? "," : "", m.sig[i]);
  o.add(']');
  if (!strcmp(m.cls, "i2c")) heapAddf(o, ",\"drv\":\"%s\",\"addr\":%d,\"freq\":%d", m.drv, m.addr, m.fast ? 400000 : 100000);
  if (!strcmp(m.cls, "sd")) heapAddf(o, ",\"khz\":%d", m.khz);
  heapAddf(o, ",\"app\":%d,\"u\":\"", m.hasApp ? 1 : 0); addJ(o, m.use); o.add('"');
}
static void wiresJson(const ModInfo& m, HeapBuf& o) {      // fili colorati: l = pin della periferica, d = destinazione, c = #rrggbb, cn = nome base del colore
  o.add('[');
  for (int i = 0; i < m.nwp; i++)
    heapAddf(o, "%s{\"l\":\"%s\",\"d\":\"%s\",\"c\":\"%s\",\"cn\":\"%s\"}", i ? "," : "", m.wp[i].lab, m.wp[i].dst, m.wp[i].hex, m.wp[i].cname);
  o.add(']');
}

bool modListJson(HeapBuf& out) {
  char (*names)[MOD_NAME_BUF] = new char[MOD_LIST_MAX][MOD_NAME_BUF];
  int total = modScan(names, MOD_LIST_MAX), n = total < MOD_LIST_MAX ? total : MOD_LIST_MAX;
  ModInfo* m = new ModInfo();
  heapAddf(out, "{\"ok\":true,\"total\":%d,\"list\":[", total);
  for (int i = 0; i < n; i++) {
    ErrBuf err;
    if (i) out.add(',');
    if (load(names[i], *m, err)) {
      out.add('{'); headJson(*m, out); out.add(",\"ok\":true,\"icon\":\""); addJ(out, m->icon); out.add("\"}");
    } else {
      out.add("{\"n\":\""); addJ(out, names[i]); out.add("\",\"ok\":false,\"e\":\""); addJ(out, err.c_str()); out.add("\"}");
    }
  }
  delete m; delete[] names;
  out.add("]}");
  return !full(out);
}

static void partsJson(const ModInfo& m, HeapBuf& o) {
  o.add('[');
  for (int i = 0; i < m.npart; i++) {
    heapAddf(o, "%s{\"k\":\"%s\",\"v\":\"", i ? "," : "", m.part[i].pos == 's' ? "serie" : m.part[i].pos == 'g' ? "gnd" : "vcc");
    addJ(o, m.part[i].val); heapAddf(o, "\",\"s\":\"%s\"}", m.part[i].sig);
  }
  o.add(']');
}

bool modInfoJson(const char* name, HeapBuf& out) {
  ModInfo* m = new ModInfo(); ErrBuf err;
  if (!load(name, *m, err)) {
    delete m;
    out.add("{\"ok\":false,\"err\":\""); addJ(out, err.c_str()); out.add("\"}");
    return !full(out);
  }
  int pins[MOD_SIG_MAX]; uint8_t c = 0; ioParseCls(m->cls, c);
  int k = ioSuggest(c, m->nsig, pins);
  out.add("{\"ok\":true,"); headJson(*m, out);
  out.add(",\"icon\":\""); addJ(out, m->icon); out.add("\",\"wiring\":\""); addJ(out, m->wiring);
  heapAddf(out, "\",\"lv\":\"%s\",\"parts\":", m->level == '5' ? "5V" : "3V3"); partsJson(*m, out);
  out.add(",\"pins\":"); wiresJson(*m, out);
  heapAddf(out, ",\"free\":%d,\"sug\":[", k >= m->nsig ? 1 : 0);
  for (int i = 0; i < k; i++) heapAddf(out, "%s%d", i ? "," : "", pins[i]);
  delete m;
  out.add("]}");
  return !full(out);
}

// "10,11,12,13" -> numeri dei pin scelti dall'utente. Ritorna quanti sono (0 = testo vuoto), -1 = testo non valido. Non controlla i pin: lo fa vos_io.
static int pinsFromText(const char* csv, int* pins, int mx) {
  int k = 0;
  for (const char* p = csv ? csv : ""; *p;) {
    if (*p == ' ' || *p == ',') { p++; continue; }
    if (*p < '0' || *p > '9' || k >= mx) return -1;
    int v = 0, digits = 0;
    for (; *p >= '0' && *p <= '9'; p++) { v = v * 10 + (*p - '0'); if (++digits > 3) return -1; }
    pins[k++] = v;
  }
  return k;
}

bool modInstall(const char* name, const char* prefixIn, const char* pinsCsv, HeapBuf& outJson, ErrBuf& err) {
  ModInfo* m = new ModInfo();
  if (!load(name, *m, err)) { delete m; return false; }
  uint8_t c = 0; ioParseCls(m->cls, c);
  int pins[MOD_SIG_MAX];
  int chosen = pinsFromText(pinsCsv, pins, MOD_SIG_MAX);                    // pin scelti dall'utente, altrimenti quelli proposti
  if (chosen < 0 || (chosen > 0 && chosen != m->nsig)) { err = tr("Pin non validi: servono tanti numeri quanti sono i segnali"); delete m; return false; }
  if (chosen == 0 && ioSuggest(c, m->nsig, pins) < m->nsig) { err = tr("Non ci sono abbastanza pin liberi per questo modulo"); delete m; return false; }
  char prefix[112];                                             // prefisso: senza spazi ai lati, minuscolo
  if (!viewCopyLower(prefix, sizeof prefix, trimView(std::string_view(prefixIn ? prefixIn : "")))) { err = tr("Nome non valido"); delete m; return false; }
  if (!strcmp(m->cls, "i2c") || !strcmp(m->cls, "sd")) {        // un solo dispositivo su piu pin: sensore I2C (SDA, SCL) o lettore microSD (CS, MOSI, SCK, MISO), nell'ordine di [signals]
    char out[16] = "", eb[IO_ERR_MAX] = "";
    bool isSd = !strcmp(m->cls, "sd");
    if (isSd ? (prefix[0] && !ioNameOk(prefix, err)) : false) { delete m; return false; }
    bool done = isSd ? ioAddSd(pins[0], pins[1], pins[2], pins[3], m->khz, prefix, out, eb) : ioAddI2c(m->drv, pins[0], pins[1], m->addr, m->fast, prefix, out, eb);
    if (!done) { err = eb; delete m; return false; }
#if VOS_WITH_SD
    if (isSd) sdAutoStart();                                    // da ora la scheda si monta da sola (il task nasce solo se c'e una microSD configurata)
#endif
    vlog(isSd ? "MOD: %s installato (microSD)" : "MOD: %s installato (sensore I2C)", m->name);
    HeapBuf res(512);
    res.add("{\"ok\":true,\"devs\":[");
    for (int i = 0; i < m->nsig; i++) heapAddf(res, "%s{\"s\":\"%s\",\"n\":\"%s\",\"g\":%d}", i ? "," : "", m->sig[i], out, pins[i]);
    res.add("]}");
    delete m;
    if (full(res)) { err = tr("Memoria insufficiente"); return false; }
    return outJson.add(res.c_str(), res.length());
  }
  if (ioCount() + m->nsig > IO_MAX) { err = tr("Troppe periferiche (massimo 16)"); delete m; return false; }
  char opt[16] = "";                                            // opzioni: inv, up, down
  if (m->low) strcat(opt, "inv,");
  if (m->pull == 'u') strcat(opt, "up,"); else if (m->pull == 'd') strcat(opt, "down,");
  char made[MOD_SIG_MAX][16]; int nm = 0; bool ok = true;
  HeapBuf res(1024);
  res.add("{\"ok\":true,\"devs\":[");
  for (int i = 0; i < m->nsig && ok; i++) {
    char nm1[128] = "";                                         // nome della periferica: prefisso, con il numero se i segnali sono piu d'uno
    if (prefix[0]) { if (m->nsig == 1) snprintf(nm1, sizeof nm1, "%s", prefix); else snprintf(nm1, sizeof nm1, "%s%d", prefix, i + 1); }
    char out[16];
    if (!ioAdd(m->cls, pins[i], nm1, opt, out, err)) { ok = false; break; }
    strcpy(made[nm++], out);
    heapAddf(res, "%s{\"s\":\"%s\",\"n\":\"%s\",\"g\":%d}", i ? "," : "", m->sig[i], out, pins[i]);
  }
  if (!ok) { ErrBuf e2; for (int i = 0; i < nm; i++) ioRemove(made[i], e2); delete m; return false; }   // tutto o niente
  vlog("MOD: %s installato (%d periferiche)", m->name, m->nsig);
  delete m;
  res.add("]}");
  if (full(res)) { err = tr("Memoria insufficiente"); return false; }
  return outJson.add(res.c_str(), res.length());
}

bool modSave(const char* name, std::string_view text, ErrBuf& err) {
  ModInfo* m = new ModInfo();
  bool ok = utilNameOk(name, strlen(name)) ? modParse(text, name, *m, err) : (err = tr("Nome non valido"), false);
  delete m;
  if (!ok) return false;
  if (hasVde(name)) { err = tr("Esiste gia un driver con questo nome"); return false; }
  ErrBuf e0; fsMkdir(MOD_DIR, e0);
  char path[64];
  snprintf(path, sizeof path, "%s/%s%s", MOD_DIR, name, MOD_EXT);              // si scrive sempre .vdb
  if (!fsWriteText(path, text, err, false)) return false;
  bool hasNew, hasOld; whichFiles(name, hasNew, hasOld);
  if (hasOld) { char old[64]; ErrBuf e; snprintf(old, sizeof old, "%s/%s%s", MOD_DIR, name, MOD_EXT_OLD); fsRemove(old, e); }   // il vecchio .vdp con lo stesso nome non serve piu
  return true;
}
bool modDelete(const char* name, ErrBuf& err) {
  if (!utilNameOk(name, strlen(name))) { err = tr("Nome non valido"); return false; }
  bool hasNew, hasOld; whichFiles(name, hasNew, hasOld);
  char path[64]; bool ok = false;
  if (hasNew) { snprintf(path, sizeof path, "%s/%s%s", MOD_DIR, name, MOD_EXT); ok = fsRemove(path, err); }
  if (hasOld) { snprintf(path, sizeof path, "%s/%s%s", MOD_DIR, name, MOD_EXT_OLD); ok = fsRemove(path, err) || ok; }
  if (!hasNew && !hasOld) err = tr("File non trovato");
  return ok;
}

void modListText(HeapBuf& out) {
  char (*names)[MOD_NAME_BUF] = new char[MOD_LIST_MAX][MOD_NAME_BUF];
  int total = modScan(names, MOD_LIST_MAX), n = total < MOD_LIST_MAX ? total : MOD_LIST_MAX;
  if (!total) { out.add(tr("Nessun driver vdb in /devs")); out.add('\n'); }
  else {
    ModInfo* m = new ModInfo();
    for (int i = 0; i < n; i++) {
      ErrBuf err;
      if (load(names[i], *m, err)) heapAddf(out, "%-24s %-5s %-5s %s x%d  OK\n", names[i], "vdb", m->cls, m->title, m->nsig);
      else heapAddf(out, "%-24s %-5s %s: %s\n", names[i], "vdb", tr("ERRORE"), err.c_str());
    }
    delete m;
  }
  delete[] names;
}
void modInfoText(const char* name, HeapBuf& out) {
  ModInfo* m = new ModInfo(); ErrBuf err;
  if (!load(name, *m, err)) heapAddf(out, "%s: %s\n", tr("ERRORE"), err.c_str());
  else {
    uint8_t c = 0; ioParseCls(m->cls, c); int pins[MOD_SIG_MAX]; int k = ioSuggest(c, m->nsig, pins);
    heapAddf(out, "%s (%s)  v%s\n", m->title, m->name, m->ver);
    heapAddf(out, "%s: %s  %s: %s%s\n", tr("Classe del modulo"), m->cls, tr("Alimentazione"), m->power, m->low ? "  active=low" : "");
    for (int i = 0; i < m->nsig; i++) {
      if (i < k) heapAddf(out, "  %s -> GPIO%d\n", m->sig[i], pins[i]);
      else heapAddf(out, "  %s -> ?\n", m->sig[i]);
    }
    heapAddf(out, "%s: %s\n", tr("Livello del segnale"), m->level == '5' ? "5 V" : "3,3 V");
    if (!strcmp(m->cls, "i2c")) heapAddf(out, "I2C: %s  0x%x  %s\n", m->drv, m->addr, m->fast ? "400 kHz" : "100 kHz");
    if (!strcmp(m->cls, "sd")) heapAddf(out, "SPI: %d kHz\n", m->khz);
    if (m->hasApp) heapAddf(out, "%s: %s  (%s: %s)\n", tr("App del driver"), m->name, tr("Permessi"), m->use[0] ? m->use : "-");
    for (int i = 0; i < m->nwp; i++) {
      heapAddf(out, "%s: %s -> %s  %s", tr("Filo"), m->wp[i].lab, m->wp[i].dst, m->wp[i].hex);
      if (m->wp[i].cname[0]) heapAddf(out, " (%s)", m->wp[i].cname);
      out.add('\n');
    }
    for (int i = 0; i < m->npart; i++) heapAddf(out, "%s: %s (%s) %s\n", tr("Componente"), m->part[i].val, m->part[i].pos == 's' ? "serie" : m->part[i].pos == 'g' ? "GND" : "VCC", m->part[i].sig);
    if (m->warn[0]) heapAddf(out, "%s: %s\n", tr("Avviso del modulo"), m->warn);
    if (k < m->nsig) { out.add(tr("Non ci sono abbastanza pin liberi per questo modulo")); out.add('\n'); }
  }
  delete m;
}
