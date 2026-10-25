// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_mod.cpp (vedi vos_mod.h): lettura e controllo delle schede modulo .vdp, pin proposti, installazione.
#include "vos_mod.h"
#include <strings.h>
#include "vos_io.h"
#include "../core/vos_i18n.h"
#include "../core/vos_log.h"
#include "../core/vos_util.h"
#include "../core/vos_files.h"
#include "../core/vos_vdesc.h"

static bool txtOk(std::string_view s, size_t mx) {
  if (s.size() < 1 || s.size() > mx) return false;
  for (char ch : s) { unsigned char c = (unsigned char)ch; if (c < 32 || c == 127) return false; }
  return true;
}
static bool sigOk(std::string_view s) {
  if (s.size() < 1 || s.size() > 8) return false;
  for (char c : s) { if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_')) return false; }
  return true;
}
// Toglie gli spazi ai lati di un testo in un buffer (come trim di Arduino). Ritorna la nuova lunghezza.
static size_t trimBuf(char* b) {
  std::string_view t = trimView(std::string_view(b));
  memmove(b, t.data(), t.size());
  b[t.size()] = 0;
  return t.size();
}

// colori dei fili: nomi in inglese e italiano; cname e il nome base (la pagina lo traduce per chi non vede i colori)
bool modColor(const char* vIn, char* hex, char* cname) {
  static const struct { const char* n; const char* alias; const char* hex; } K[] = {
    {"red", "rosso", "#e53935"}, {"black", "nero", "#212121"}, {"blue", "blu", "#1e88e5"}, {"green", "verde", "#43a047"},
    {"yellow", "giallo", "#fdd835"}, {"orange", "arancio", "#fb8c00"}, {"white", "bianco", "#f5f5f5"}, {"brown", "marrone", "#6d4c41"},
    {"purple", "viola", "#8e24aa"}, {"gray", "grigio", "#9e9e9e"}, {"pink", "rosa", "#ec407a"}, {"cyan", "azzurro", "#00acc1"}};
  char v[16]; size_t n = 0;
  while (*vIn == ' ' || *vIn == '\t') vIn++;
  for (; *vIn && n < sizeof v - 1; vIn++) v[n++] = (*vIn >= 'A' && *vIn <= 'Z') ? (char)(*vIn + 32) : *vIn;
  if (*vIn) return false;
  while (n && (v[n - 1] == ' ' || v[n - 1] == '\t' || v[n - 1] == '\r')) n--;
  v[n] = 0;
  if (!strcmp(v, "arancione")) strcpy(v, "arancio");
  if (!strcmp(v, "grey")) strcpy(v, "gray");
  for (size_t i = 0; i < sizeof(K) / sizeof(K[0]); i++)
    if (!strcmp(v, K[i].n) || !strcmp(v, K[i].alias)) { strcpy(hex, K[i].hex); strcpy(cname, K[i].n); return true; }
  if (n == 7 && v[0] == '#') {
    for (int i = 1; i < 7; i++) if (!((v[i] >= '0' && v[i] <= '9') || (v[i] >= 'a' && v[i] <= 'f'))) return false;
    strcpy(hex, v); cname[0] = 0; return true;
  }
  return false;
}

bool modParse(std::string_view text, const char* fileName, ModInfo& m, ErrBuf& err) {
  memset(&m, 0, sizeof(m));
  strcpy(m.power, "-"); strcpy(m.ver, "1");
  m.pull = 'n'; m.level = '3'; m.size = text.size(); err = "";
  if (text.size() > MOD_FILE_MAX) { err = tr("File troppo grande (massimo 8 KB)"); return false; }
  enum { NONE, MOD, SIG, ICO, WIR, PIN };
  int sec = NONE; bool seen[6] = {false, false, false, false, false, false}; bool hasPull = false; bool hasFreq = false;
  size_t iconLen = 0, wiringLen = 0;                                 // m.icon e m.wiring si riempiono riga per riga
  LineIter lines(text);
  std::string_view raw;
  while (lines.next(raw)) {
    int line = lines.line();
    std::string_view t = trimView(raw);
    if (t.empty() || t[0] == '#') continue;
    if (t.size() > 1 && t[0] == '[' && t.back() == ']') {
      std::string_view s = t.substr(1, t.size() - 2);
      int ns = viewEqI(s, "module") ? MOD : viewEqI(s, "signals") ? SIG : viewEqI(s, "icon") ? ICO : viewEqI(s, "wiring") ? WIR : viewEqI(s, "pins") ? PIN : NONE;
      if (ns == NONE) return utilLineErr(err, line, tr("Sezione sconosciuta (ammesse: module, signals, icon, wiring, pins)"));
      if (seen[ns]) return utilLineErr(err, line, tr("Sezione ripetuta"));
      seen[ns] = true; sec = ns; continue;
    }
    if (sec == NONE) return utilLineErr(err, line, tr("Riga fuori da una sezione"));
    if (sec == ICO) {
      if (iconLen + t.size() + 1 > MOD_ICON_MAX + 8) return utilLineErr(err, line, tr("Icona non ammessa (un solo svg con forme semplici, massimo 2 KB)"));
      bufAppend(m.icon, sizeof m.icon, iconLen, t); bufAppend(m.icon, sizeof m.icon, iconLen, "\n");
      continue;
    }
    if (sec == WIR) {
      if (wiringLen + t.size() + 1 > MOD_WIRING_MAX + 8) return utilLineErr(err, line, tr("Schema non ammesso (un solo svg con forme semplici, massimo 4 KB)"));
      bufAppend(m.wiring, sizeof m.wiring, wiringLen, t); bufAppend(m.wiring, sizeof m.wiring, wiringLen, "\n");
      continue;
    }
    if (sec == SIG) {
      if (!sigOk(t)) return utilLineErr(err, line, tr("Segnale non valido (1-8 caratteri: lettere, numeri, _)"));
      if (m.nsig >= MOD_SIG_MAX) return utilLineErr(err, line, tr("Troppi segnali (massimo 8)"));
      for (int i = 0; i < m.nsig; i++) if (viewEqI(t, m.sig[i])) return utilLineErr(err, line, tr("Segnale ripetuto"));
      viewCopy(m.sig[m.nsig], sizeof m.sig[0], t); m.nsig++; continue;
    }
    size_t q = t.find('=');
    if (q == std::string_view::npos || q < 1) return utilLineErr(err, line, tr("Serve chiave=valore"));
    if (sec == PIN) {
      std::string_view lab = trimView(t.substr(0, q)), rest = trimView(t.substr(q + 1));
      size_t cm = rest.find(',');
      if (cm == std::string_view::npos || cm < 1) return utilLineErr(err, line, tr("pins: ETICHETTA=destinazione,colore"));
      std::string_view dst = trimView(rest.substr(0, cm)), col = trimView(rest.substr(cm + 1));
      if (!sigOk(lab) || !sigOk(dst)) return utilLineErr(err, line, tr("pins: ETICHETTA=destinazione,colore"));
      if (m.nwp >= MOD_PINS_MAX) return utilLineErr(err, line, tr("Troppi fili in [pins] (massimo 8)"));
      for (int i = 0; i < m.nwp; i++) if (viewEqI(lab, m.wp[i].lab)) return utilLineErr(err, line, tr("Pin ripetuto in [pins]"));
      ModInfo::WirePin& w = m.wp[m.nwp];
      char colb[24];                                                 // un colore piu lungo non e comunque valido
      if (!viewCopy(colb, sizeof colb, col) || !modColor(colb, w.hex, w.cname)) return utilLineErr(err, line, tr("Colore non valido (nome come red, rosso, nero, oppure #rrggbb)"));
      viewCopy(w.lab, sizeof w.lab, lab); viewCopy(w.dst, sizeof w.dst, dst);
      m.nwp++; continue;
    }
    std::string_view k = trimView(t.substr(0, q)), v = trimView(t.substr(q + 1));
    char vb[96];                                                     // il valore con lo zero finale, per class= e addr=: uno piu lungo non e comunque valido
    bool vfit = viewCopy(vb, sizeof vb, v);
    if (viewEqI(k, "name")) { if (!utilNameOk(v.data(), v.size()) || v != std::string_view(fileName)) return utilLineErr(err, line, tr("Nome non valido o diverso dal nome del file")); viewCopy(m.name, sizeof m.name, v); }
    else if (viewEqI(k, "title")) { if (!txtOk(v, 32)) return utilLineErr(err, line, tr("Testo non valido (1-32 caratteri, niente caratteri di controllo)")); viewCopy(m.title, sizeof m.title, v); }
    else if (viewEqI(k, "class")) {
      uint8_t c; if (!vfit || !ioParseCls(vb, c) || !viewCopy(m.cls, sizeof m.cls, v)) return utilLineErr(err, line, tr("Classe non valida (rele, led, pin, ingresso, analogico, i2c)"));
    }
    else if (viewEqI(k, "driver")) { if (v != "bmp280") return utilLineErr(err, line, tr("Driver I2C sconosciuto (ammesso: bmp280)")); strcpy(m.drv, "bmp280"); }
    else if (viewEqI(k, "addr")) {
      char* e = nullptr; long a = vfit ? strtol(vb, &e, 0) : 0;
      if (!vfit || v.empty() || *e || a < 0x08 || a > 0x77) return utilLineErr(err, line, tr("Indirizzo I2C non valido (da 0x08 a 0x77)"));
      m.addr = (int)a;
    }
    else if (viewEqI(k, "freq")) { if (v != "100000" && v != "400000") return utilLineErr(err, line, tr("freq: 100000 oppure 400000")); m.fast = (v == "400000"); hasFreq = true; }
    else if (viewEqI(k, "active")) { if (v != "low" && v != "high") return utilLineErr(err, line, tr("active: low oppure high")); m.low = (v == "low"); }
    else if (viewEqI(k, "pull")) { if (v != "none" && v != "up" && v != "down") return utilLineErr(err, line, tr("pull: none, up oppure down")); m.pull = v == "up" ? 'u' : v == "down" ? 'd' : 'n'; hasPull = true; }
    else if (viewEqI(k, "power")) {
      if (!viewEqI(v, "3V3") && !viewEqI(v, "5V") && !viewEqI(v, "12V") && !viewEqI(v, "24V") && !viewEqI(v, "230V") && !viewEqI(v, "NONE")) return utilLineErr(err, line, tr("power: 3V3, 5V, 12V, 24V, 230V oppure none"));
      if (viewEqI(v, "NONE")) strcpy(m.power, "-"); else viewCopyUpper(m.power, sizeof m.power, v);
    }
    else if (viewEqI(k, "level")) { if (!viewEqI(v, "3V3") && !viewEqI(v, "5V")) return utilLineErr(err, line, tr("level: 3V3 oppure 5V")); m.level = viewEqI(v, "5V") ? '5' : '3'; }
    else if (viewEqI(k, "part")) {
      size_t c1 = v.find(','), c2 = v.rfind(',');
      if (c1 == std::string_view::npos || c1 < 1 || c2 <= c1) return utilLineErr(err, line, tr("part: serie|gnd|vcc,valore,segnale"));
      std::string_view pp = trimView(v.substr(0, c1)), pv = trimView(v.substr(c1 + 1, c2 - c1 - 1)), ps = trimView(v.substr(c2 + 1));
      if (!viewEqI(pp, "serie") && !viewEqI(pp, "gnd") && !viewEqI(pp, "vcc")) return utilLineErr(err, line, tr("part: serie|gnd|vcc,valore,segnale"));
      if (!txtOk(pv, 12) || pv.find('"') != std::string_view::npos || pv.find('\\') != std::string_view::npos || pv.find('<') != std::string_view::npos) return utilLineErr(err, line, tr("Valore del componente non valido (1-12 caratteri)"));
      if (!sigOk(ps)) return utilLineErr(err, line, tr("Segnale non valido (1-8 caratteri: lettere, numeri, _)"));
      if (m.npart >= MOD_PART_MAX) return utilLineErr(err, line, tr("Troppi componenti (massimo 4)"));
      m.part[m.npart].pos = viewEqI(pp, "serie") ? 's' : viewEqI(pp, "gnd") ? 'g' : 'v';
      viewCopy(m.part[m.npart].val, sizeof m.part[0].val, pv);
      viewCopy(m.part[m.npart].sig, sizeof m.part[0].sig, ps); m.npart++;
    }
    else if (viewEqI(k, "author")) { if (!txtOk(v, 24)) return utilLineErr(err, line, tr("Testo non valido (1-24 caratteri, niente caratteri di controllo)")); viewCopy(m.author, sizeof m.author, v); }
    else if (viewEqI(k, "license")) { if (!txtOk(v, 24)) return utilLineErr(err, line, tr("Testo non valido (1-24 caratteri, niente caratteri di controllo)")); viewCopy(m.lic, sizeof m.lic, v); }
    else if (viewEqI(k, "warn")) { if (!txtOk(v, 80)) return utilLineErr(err, line, tr("Avviso non valido (1-80 caratteri)")); viewCopy(m.warn, sizeof m.warn, v); }
    else if (viewEqI(k, "version")) {
      bool ok = v.size() >= 1 && v.size() <= 10;
      for (size_t i = 0; ok && i < v.size(); i++) if (!((v[i] >= '0' && v[i] <= '9') || v[i] == '.')) ok = false;
      if (!ok) return utilLineErr(err, line, tr("Versione non valida (solo cifre e punti, massimo 10)"));
      viewCopy(m.ver, sizeof m.ver, v);
    }
    else return utilLineErr(err, line, tr("Chiave sconosciuta"));
  }
  if (!seen[MOD] || m.name[0] == 0) { err = tr("Manca [module] con name="); return false; }
  if (!m.cls[0]) { err = tr("Manca class="); return false; }
  if (m.nsig == 0) { err = tr("Manca la sezione [signals] con almeno un segnale"); return false; }
  for (int i = 0; i < m.npart; i++) {            // ogni componente deve riferirsi a un segnale del modulo
    bool f = false; for (int s = 0; s < m.nsig; s++) if (!strcasecmp(m.part[i].sig, m.sig[s])) { f = true; strncpy(m.part[i].sig, m.sig[s], 8); }
    if (!f) { err = tr("part: il segnale non e nella sezione [signals]"); return false; }
  }
  if (!strcmp(m.cls, "i2c")) {
    if (!m.drv[0]) { err = tr("Manca driver= (classe i2c)"); return false; }
    if (m.addr != 0x76 && m.addr != 0x77) { err = tr("Indirizzo I2C del BMP280: 0x76 oppure 0x77"); return false; }
    if (m.nsig != 2 || strcmp(m.sig[0], "SDA") || strcmp(m.sig[1], "SCL")) { err = tr("Classe i2c: i segnali sono SDA e SCL, in questo ordine"); return false; }
    if (m.low || hasPull) { err = tr("active e pull non valgono per la classe i2c"); return false; }
  } else if (m.drv[0] || m.addr || hasFreq) { err = tr("driver, addr e freq valgono solo per la classe i2c"); return false; }
  if (seen[PIN]) {                                  // ogni filo va a vcc, gnd o a un segnale del modulo; ogni segnale deve avere il suo filo
    bool wired[MOD_SIG_MAX] = {false};
    for (int i = 0; i < m.nwp; i++) {
      char* d = m.wp[i].dst;
      if (!strcasecmp(d, "vcc") || !strcasecmp(d, "gnd")) { strcpy(d, !strcasecmp(d, "vcc") ? "VCC" : "GND"); continue; }
      bool f = false; for (int s = 0; s < m.nsig; s++) if (!strcasecmp(d, m.sig[s])) { f = true; wired[s] = true; strncpy(m.wp[i].dst, m.sig[s], 8); }
      if (!f) { err = tr("pins: la destinazione non e vcc, gnd o un segnale di [signals]"); return false; }
    }
    for (int s = 0; s < m.nsig; s++) if (!wired[s]) { err = tr("pins: manca il filo di un segnale"); return false; }
  }
  if (hasPull && strcmp(m.cls, "ingresso")) { err = tr("pull vale solo per la classe ingresso"); return false; }
  if (m.low && !strcmp(m.cls, "analogico")) { err = tr("active=low non vale per la classe analogico"); return false; }
  if (m.title[0] == 0) strcpy(m.title, m.name);
  if (seen[ICO]) { trimBuf(m.icon); if (!utilSvgSafe(m.icon, MOD_ICON_MAX)) { err = tr("Icona non ammessa (un solo svg con forme semplici, massimo 2 KB)"); return false; } } else m.icon[0] = 0;
  if (seen[WIR]) { trimBuf(m.wiring); if (!utilSvgSafe(m.wiring, MOD_WIRING_MAX)) { err = tr("Schema non ammesso (un solo svg con forme semplici, massimo 4 KB)"); return false; } } else m.wiring[0] = 0;
  return true;
}


// ---------- elenco dei file /devs/*.vdp ----------
// Scrive in names (mx nomi da MOD_NAME_BUF byte) i primi nomi delle schede e ritorna quante ce ne sono in tutto. Crea la cartella se manca.
#define MOD_NAME_BUF 48
static int scan(char (*names)[MOD_NAME_BUF], int mx) {
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
    if (isDir || name.size() < 4 || name.substr(name.size() - 4) != MOD_EXT) continue;
    if (n < mx) {                                                  // un nome troppo lungo si taglia: resta comunque non valido
      size_t k = name.size() - 4 < MOD_NAME_BUF - 1 ? name.size() - 4 : MOD_NAME_BUF - 1;
      memcpy(names[n], name.data(), k); names[n][k] = 0;
    }
    n++;
  }
  return n;
}
// Percorso del file di una scheda modulo (il nome e gia valido: al massimo 24 caratteri).
static void modPath(char* out, size_t cap, const char* name) { snprintf(out, cap, "%s/%s%s", MOD_DIR, name, MOD_EXT); }

static bool load(const char* name, ModInfo& m, ErrBuf& err) {
  HeapBuf text(fsTextMax()); ErrBuf e;
  if (!utilNameOk(name, strlen(name))) { err = tr("Nome non valido"); return false; }
  char path[64];
  modPath(path, sizeof path, name);
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
}
static void wiresJson(const ModInfo& m, HeapBuf& o) {      // fili colorati: l = pin della periferica, d = destinazione, c = #rrggbb, cn = nome base del colore
  o.add('[');
  for (int i = 0; i < m.nwp; i++)
    heapAddf(o, "%s{\"l\":\"%s\",\"d\":\"%s\",\"c\":\"%s\",\"cn\":\"%s\"}", i ? "," : "", m.wp[i].lab, m.wp[i].dst, m.wp[i].hex, m.wp[i].cname);
  o.add(']');
}

bool modListJson(HeapBuf& out) {
  char (*names)[MOD_NAME_BUF] = new char[MOD_LIST_MAX][MOD_NAME_BUF];
  int total = scan(names, MOD_LIST_MAX), n = total < MOD_LIST_MAX ? total : MOD_LIST_MAX;
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

bool modInstall(const char* name, const char* prefixIn, HeapBuf& outJson, ErrBuf& err) {
  ModInfo* m = new ModInfo();
  if (!load(name, *m, err)) { delete m; return false; }
  uint8_t c = 0; ioParseCls(m->cls, c);
  int pins[MOD_SIG_MAX]; int k = ioSuggest(c, m->nsig, pins);
  if (k < m->nsig) { err = tr("Non ci sono abbastanza pin liberi per questo modulo"); delete m; return false; }
  char prefix[112];                                             // prefisso: senza spazi ai lati, minuscolo
  if (!viewCopyLower(prefix, sizeof prefix, trimView(std::string_view(prefixIn ? prefixIn : "")))) { err = tr("Nome non valido"); delete m; return false; }
  if (!strcmp(m->cls, "i2c")) {                                 // un solo sensore su due pin: SDA e SCL (in questo ordine) prendono i primi due pin proposti
    int sda = pins[0], scl = pins[1];
    char out[16] = "", eb[IO_ERR_MAX] = "";
    if (!ioAddI2c(m->drv, sda, scl, m->addr, m->fast, prefix, out, eb)) { err = eb; delete m; return false; }
    vlog("MOD: %s installato (sensore I2C)", m->name);
    char js[128];                                               // 2 nomi da 15 + 2 numeri: sta largo, per non tagliare mai la risposta
    snprintf(js, sizeof js, "{\"ok\":true,\"devs\":[{\"s\":\"SDA\",\"n\":\"%s\",\"g\":%d},{\"s\":\"SCL\",\"n\":\"%s\",\"g\":%d}]}", out, sda, out, scl);
    delete m;
    return outJson.add(js);
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
  ErrBuf e0; fsMkdir(MOD_DIR, e0);
  char path[64];
  modPath(path, sizeof path, name);
  return fsWriteText(path, text, err, false);
}
bool modDelete(const char* name, ErrBuf& err) {
  if (!utilNameOk(name, strlen(name))) { err = tr("Nome non valido"); return false; }
  char path[64];
  modPath(path, sizeof path, name);
  return fsRemove(path, err);
}

void modListText(HeapBuf& out) {
  char (*names)[MOD_NAME_BUF] = new char[MOD_LIST_MAX][MOD_NAME_BUF];
  int total = scan(names, MOD_LIST_MAX), n = total < MOD_LIST_MAX ? total : MOD_LIST_MAX;
  if (!total) { out.add(tr("Nessuna scheda modulo in /devs. Si caricano dalla pagina Periferiche")); out.add('\n'); }
  else {
    heapAddf(out, tr("Schede modulo in /devs: %d"), total); out.add('\n');
    ModInfo* m = new ModInfo();
    for (int i = 0; i < n; i++) {
      ErrBuf err;
      if (load(names[i], *m, err)) heapAddf(out, "  %s  %s  %s x%d  OK\n", names[i], m->title, m->cls, m->nsig);
      else heapAddf(out, "  %s  %s: %s\n", names[i], tr("ERRORE"), err.c_str());
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
