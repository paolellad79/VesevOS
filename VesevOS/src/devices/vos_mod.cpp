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

static bool txtOk(const String& s, size_t mx) {
  if (s.length() < 1 || s.length() > mx) return false;
  for (size_t i = 0; i < s.length(); i++) { unsigned char c = (unsigned char)s[i]; if (c < 32 || c == 127) return false; }
  return true;
}
static bool sigOk(const String& s) {
  if (s.length() < 1 || s.length() > 8) return false;
  for (size_t i = 0; i < s.length(); i++) { char c = s[i]; if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_')) return false; }
  return true;
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

bool modParse(const String& text, const String& fileName, ModInfo& m, String& err) {
  m.name = ""; m.title = ""; m.cls = ""; m.power = "-"; m.author = ""; m.ver = "1"; m.lic = ""; m.warn = ""; m.icon = ""; m.wiring = "";
  m.low = false; m.pull = 'n'; m.nsig = 0; m.level = '3'; m.npart = 0; m.size = text.length(); err = "";
  m.drv[0] = 0; m.addr = 0; m.fast = false; m.nwp = 0;
  if (text.length() > MOD_FILE_MAX) { err = tr("File troppo grande (massimo 8 KB)"); return false; }
  enum { NONE, MOD, SIG, ICO, WIR, PIN };
  int sec = NONE; bool seen[6] = {false, false, false, false, false, false}; bool hasPull = false; bool hasFreq = false;
  size_t pos = 0; int line = 0;
  while (pos <= text.length()) {
    int e = text.indexOf('\n', pos); if (e < 0) e = text.length();
    String raw = text.substring(pos, e); pos = e + 1; line++;
    if (raw.endsWith("\r")) raw.remove(raw.length() - 1);
    String t = raw; t.trim();
    if (t.length() == 0 || t[0] == '#') continue;
    if (t.length() > 1 && t[0] == '[' && t[t.length() - 1] == ']') {
      String s = t.substring(1, t.length() - 1); s.toLowerCase();
      int ns = s == "module" ? MOD : s == "signals" ? SIG : s == "icon" ? ICO : s == "wiring" ? WIR : s == "pins" ? PIN : NONE;
      if (ns == NONE) return utilLineErr(err, line, tr("Sezione sconosciuta (ammesse: module, signals, icon, wiring, pins)"));
      if (seen[ns]) return utilLineErr(err, line, tr("Sezione ripetuta"));
      seen[ns] = true; sec = ns; continue;
    }
    if (sec == NONE) return utilLineErr(err, line, tr("Riga fuori da una sezione"));
    if (sec == ICO) { m.icon += t; m.icon += "\n"; if (m.icon.length() > MOD_ICON_MAX + 8) return utilLineErr(err, line, tr("Icona non ammessa (un solo svg con forme semplici, massimo 2 KB)")); continue; }
    if (sec == WIR) { m.wiring += t; m.wiring += "\n"; if (m.wiring.length() > MOD_WIRING_MAX + 8) return utilLineErr(err, line, tr("Schema non ammesso (un solo svg con forme semplici, massimo 4 KB)")); continue; }
    if (sec == SIG) {
      if (!sigOk(t)) return utilLineErr(err, line, tr("Segnale non valido (1-8 caratteri: lettere, numeri, _)"));
      if (m.nsig >= MOD_SIG_MAX) return utilLineErr(err, line, tr("Troppi segnali (massimo 8)"));
      for (int i = 0; i < m.nsig; i++) if (t.equalsIgnoreCase(m.sig[i])) return utilLineErr(err, line, tr("Segnale ripetuto"));
      strncpy(m.sig[m.nsig], t.c_str(), 8); m.sig[m.nsig][8] = 0; m.nsig++; continue;
    }
    int q = t.indexOf('=');
    if (q < 1) return utilLineErr(err, line, tr("Serve chiave=valore"));
    if (sec == PIN) {
      String lab = t.substring(0, q), rest = t.substring(q + 1); lab.trim(); rest.trim();
      int cm = rest.indexOf(',');
      if (cm < 1) return utilLineErr(err, line, tr("pins: ETICHETTA=destinazione,colore"));
      String dst = rest.substring(0, cm), col = rest.substring(cm + 1); dst.trim(); col.trim();
      if (!sigOk(lab) || !sigOk(dst)) return utilLineErr(err, line, tr("pins: ETICHETTA=destinazione,colore"));
      if (m.nwp >= MOD_PINS_MAX) return utilLineErr(err, line, tr("Troppi fili in [pins] (massimo 8)"));
      for (int i = 0; i < m.nwp; i++) if (lab.equalsIgnoreCase(m.wp[i].lab)) return utilLineErr(err, line, tr("Pin ripetuto in [pins]"));
      ModInfo::WirePin& w = m.wp[m.nwp];
      if (!modColor(col.c_str(), w.hex, w.cname)) return utilLineErr(err, line, tr("Colore non valido (nome come red, rosso, nero, oppure #rrggbb)"));
      strncpy(w.lab, lab.c_str(), 8); w.lab[8] = 0; strncpy(w.dst, dst.c_str(), 8); w.dst[8] = 0;
      m.nwp++; continue;
    }
    String k = t.substring(0, q), v = t.substring(q + 1); k.trim(); v.trim(); k.toLowerCase();
    if (k == "name") { if (!utilNameOk(v) || v != fileName) return utilLineErr(err, line, tr("Nome non valido o diverso dal nome del file")); m.name = v; }
    else if (k == "title") { if (!txtOk(v, 32)) return utilLineErr(err, line, tr("Testo non valido (1-32 caratteri, niente caratteri di controllo)")); m.title = v; }
    else if (k == "class") {
      uint8_t c; if (!ioParseCls(v, c)) return utilLineErr(err, line, tr("Classe non valida (rele, led, pin, ingresso, analogico, i2c)"));
      m.cls = v;
    }
    else if (k == "driver") { if (v != "bmp280") return utilLineErr(err, line, tr("Driver I2C sconosciuto (ammesso: bmp280)")); strcpy(m.drv, "bmp280"); }
    else if (k == "addr") {
      char* e = nullptr; long a = strtol(v.c_str(), &e, 0);
      if (!v.length() || *e || a < 0x08 || a > 0x77) return utilLineErr(err, line, tr("Indirizzo I2C non valido (da 0x08 a 0x77)"));
      m.addr = (int)a;
    }
    else if (k == "freq") { if (v != "100000" && v != "400000") return utilLineErr(err, line, tr("freq: 100000 oppure 400000")); m.fast = (v == "400000"); hasFreq = true; }
    else if (k == "active") { if (v != "low" && v != "high") return utilLineErr(err, line, tr("active: low oppure high")); m.low = (v == "low"); }
    else if (k == "pull") { if (v != "none" && v != "up" && v != "down") return utilLineErr(err, line, tr("pull: none, up oppure down")); m.pull = v == "up" ? 'u' : v == "down" ? 'd' : 'n'; hasPull = true; }
    else if (k == "power") {
      String u = v; u.toUpperCase();
      if (u != "3V3" && u != "5V" && u != "12V" && u != "24V" && u != "230V" && u != "NONE") return utilLineErr(err, line, tr("power: 3V3, 5V, 12V, 24V, 230V oppure none"));
      if (u == "NONE") u = "-";
      m.power = u;
    }
    else if (k == "level") { String u = v; u.toUpperCase(); if (u != "3V3" && u != "5V") return utilLineErr(err, line, tr("level: 3V3 oppure 5V")); m.level = (u == "5V") ? '5' : '3'; }
    else if (k == "part") {
      int c1 = v.indexOf(','), c2 = v.lastIndexOf(',');
      if (c1 < 1 || c2 <= c1) return utilLineErr(err, line, tr("part: serie|gnd|vcc,valore,segnale"));
      String pp = v.substring(0, c1), pv = v.substring(c1 + 1, c2), ps = v.substring(c2 + 1); pp.trim(); pv.trim(); ps.trim(); pp.toLowerCase();
      if (pp != "serie" && pp != "gnd" && pp != "vcc") return utilLineErr(err, line, tr("part: serie|gnd|vcc,valore,segnale"));
      if (!txtOk(pv, 12) || pv.indexOf('"') >= 0 || pv.indexOf('\\') >= 0 || pv.indexOf('<') >= 0) return utilLineErr(err, line, tr("Valore del componente non valido (1-12 caratteri)"));
      if (!sigOk(ps)) return utilLineErr(err, line, tr("Segnale non valido (1-8 caratteri: lettere, numeri, _)"));
      if (m.npart >= MOD_PART_MAX) return utilLineErr(err, line, tr("Troppi componenti (massimo 4)"));
      m.part[m.npart].pos = pp == "serie" ? 's' : pp == "gnd" ? 'g' : 'v';
      strncpy(m.part[m.npart].val, pv.c_str(), 12); m.part[m.npart].val[12] = 0;
      strncpy(m.part[m.npart].sig, ps.c_str(), 8); m.part[m.npart].sig[8] = 0; m.npart++;
    }
    else if (k == "author") { if (!txtOk(v, 24)) return utilLineErr(err, line, tr("Testo non valido (1-24 caratteri, niente caratteri di controllo)")); m.author = v; }
    else if (k == "license") { if (!txtOk(v, 24)) return utilLineErr(err, line, tr("Testo non valido (1-24 caratteri, niente caratteri di controllo)")); m.lic = v; }
    else if (k == "warn") { if (!txtOk(v, 80)) return utilLineErr(err, line, tr("Avviso non valido (1-80 caratteri)")); m.warn = v; }
    else if (k == "version") {
      bool ok = v.length() >= 1 && v.length() <= 10;
      for (size_t i = 0; ok && i < v.length(); i++) if (!((v[i] >= '0' && v[i] <= '9') || v[i] == '.')) ok = false;
      if (!ok) return utilLineErr(err, line, tr("Versione non valida (solo cifre e punti, massimo 10)"));
      m.ver = v;
    }
    else return utilLineErr(err, line, tr("Chiave sconosciuta"));
  }
  if (!seen[MOD] || m.name.length() == 0) { err = tr("Manca [module] con name="); return false; }
  if (!m.cls.length()) { err = tr("Manca class="); return false; }
  if (m.nsig == 0) { err = tr("Manca la sezione [signals] con almeno un segnale"); return false; }
  for (int i = 0; i < m.npart; i++) {            // ogni componente deve riferirsi a un segnale del modulo
    bool f = false; for (int s = 0; s < m.nsig; s++) if (!strcasecmp(m.part[i].sig, m.sig[s])) { f = true; strncpy(m.part[i].sig, m.sig[s], 8); }
    if (!f) { err = tr("part: il segnale non e nella sezione [signals]"); return false; }
  }
  if (m.cls == "i2c") {
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
  if (hasPull && m.cls != "ingresso") { err = tr("pull vale solo per la classe ingresso"); return false; }
  if (m.low && (m.cls == "analogico")) { err = tr("active=low non vale per la classe analogico"); return false; }
  if (m.title.length() == 0) m.title = m.name;
  if (seen[ICO]) { m.icon.trim(); if (!utilSvgSafe(m.icon, MOD_ICON_MAX)) { err = tr("Icona non ammessa (un solo svg con forme semplici, massimo 2 KB)"); return false; } } else m.icon = "";
  if (seen[WIR]) { m.wiring.trim(); if (!utilSvgSafe(m.wiring, MOD_WIRING_MAX)) { err = tr("Schema non ammesso (un solo svg con forme semplici, massimo 4 KB)"); return false; } } else m.wiring = "";
  return true;
}

// ---------- elenco dei file /devs/*.vdp ----------
static int scan(String* names, int mx) {
  String j = fsListJson(MOD_DIR);
  if (j.indexOf("\"ok\":true") < 0) { String e; fsMkdir(MOD_DIR, e); j = fsListJson(MOD_DIR); }
  int n = 0, pos = 0;
  while (true) {
    int a = j.indexOf("{\"n\":\"", pos); if (a < 0) break;
    int b = j.indexOf("\"", a + 6); if (b < 0) break;
    String name = j.substring(a + 6, b);
    int d = j.indexOf("\"d\":", b);
    bool isDir = d >= 0 && j.substring(d + 4, d + 8) == "true";
    pos = b + 1;
    if (isDir || !name.endsWith(MOD_EXT)) continue;
    if (n < mx) names[n] = name.substring(0, name.length() - 4);
    n++;
  }
  return n;
}
static bool load(const String& name, ModInfo& m, String& err) {
  String text, e;
  if (!utilNameOk(name)) { err = tr("Nome non valido"); return false; }
  if (!fsReadText(String(MOD_DIR) + "/" + name + MOD_EXT, text, e)) { err = e.length() ? e : String(tr("File non trovato")); return false; }
  return modParse(text, name, m, err);
}

static String sigJson(const ModInfo& m) {
  String s = "[";
  for (int i = 0; i < m.nsig; i++) { if (i) s += ","; s += "\"" + String(m.sig[i]) + "\""; }
  return s + "]";
}
static String headJson(const ModInfo& m) {
  return "\"n\":\"" + m.name + "\",\"t\":\"" + jsonEscape(m.title) + "\",\"c\":\"" + m.cls + "\",\"p\":\"" + m.power + "\",\"ns\":" + String(m.nsig) +
         ",\"low\":" + String(m.low ? 1 : 0) + ",\"pull\":\"" + String(m.pull) + "\",\"v\":\"" + m.ver + "\",\"a\":\"" + jsonEscape(m.author) +
         "\",\"l\":\"" + jsonEscape(m.lic) + "\",\"w\":\"" + jsonEscape(m.warn) + "\",\"sig\":" + sigJson(m) +
         (m.cls == "i2c" ? ",\"drv\":\"" + String(m.drv) + "\",\"addr\":" + String(m.addr) + ",\"freq\":" + String(m.fast ? 400000 : 100000) : String());
}
static String wiresJson(const ModInfo& m) {                // fili colorati: l = pin della periferica, d = destinazione, c = #rrggbb, cn = nome base del colore
  String j = "[";
  for (int i = 0; i < m.nwp; i++) {
    if (i) j += ",";
    j += "{\"l\":\"" + String(m.wp[i].lab) + "\",\"d\":\"" + String(m.wp[i].dst) + "\",\"c\":\"" + String(m.wp[i].hex) + "\",\"cn\":\"" + String(m.wp[i].cname) + "\"}";
  }
  return j + "]";
}

String modListJson() {
  String* names = new String[MOD_LIST_MAX];
  int total = scan(names, MOD_LIST_MAX), n = total < MOD_LIST_MAX ? total : MOD_LIST_MAX;
  ModInfo* m = new ModInfo();
  String j = "{\"ok\":true,\"total\":" + String(total) + ",\"list\":[";
  for (int i = 0; i < n; i++) {
    String err;
    if (i) j += ",";
    if (load(names[i], *m, err)) j += "{" + headJson(*m) + ",\"ok\":true,\"icon\":\"" + jsonEscape(m->icon) + "\"}";
    else j += "{\"n\":\"" + names[i] + "\",\"ok\":false,\"e\":\"" + jsonEscape(err) + "\"}";
  }
  delete m; delete[] names;
  return j + "]}";
}

static String partsJson(const ModInfo& m) {
  String j = "[";
  for (int i = 0; i < m.npart; i++) {
    if (i) j += ",";
    j += "{\"k\":\"" + String(m.part[i].pos == 's' ? "serie" : m.part[i].pos == 'g' ? "gnd" : "vcc") + "\",\"v\":\"" + jsonEscape(String(m.part[i].val)) + "\",\"s\":\"" + String(m.part[i].sig) + "\"}";
  }
  return j + "]";
}

String modInfoJson(const String& name) {
  ModInfo* m = new ModInfo(); String err;
  if (!load(name, *m, err)) { delete m; return "{\"ok\":false,\"err\":\"" + jsonEscape(err) + "\"}"; }
  int pins[MOD_SIG_MAX]; uint8_t c = 0; ioParseCls(m->cls, c);
  int k = ioSuggest(c, m->nsig, pins);
  String j = "{\"ok\":true," + headJson(*m) + ",\"icon\":\"" + jsonEscape(m->icon) + "\",\"wiring\":\"" + jsonEscape(m->wiring) + "\",\"lv\":\"" + String(m->level == '5' ? "5V" : "3V3") + "\",\"parts\":" + partsJson(*m) + ",\"pins\":" + wiresJson(*m) + ",\"free\":" + String(k >= m->nsig ? 1 : 0) + ",\"sug\":[";
  for (int i = 0; i < k; i++) { if (i) j += ","; j += String(pins[i]); }
  delete m;
  return j + "]}";
}

bool modInstall(const String& name, const String& prefixIn, String& outJson, String& err) {
  ModInfo* m = new ModInfo();
  if (!load(name, *m, err)) { delete m; return false; }
  uint8_t c = 0; ioParseCls(m->cls, c);
  int pins[MOD_SIG_MAX]; int k = ioSuggest(c, m->nsig, pins);
  if (k < m->nsig) { err = tr("Non ci sono abbastanza pin liberi per questo modulo"); delete m; return false; }
  String prefix = prefixIn; prefix.trim(); prefix.toLowerCase();
  if (m->cls == "i2c") {                                        // un solo sensore su due pin: SDA e SCL (in questo ordine) prendono i primi due pin proposti
    int sda = pins[0], scl = pins[1];
    char out[16] = "", eb[IO_ERR_MAX] = "";
    if (!ioAddI2c(m->drv, sda, scl, m->addr, m->fast, prefix.c_str(), out, eb)) { err = eb; delete m; return false; }
    vlog("MOD: %s installato (sensore I2C)", m->name.c_str());
    outJson = "{\"ok\":true,\"devs\":[{\"s\":\"SDA\",\"n\":\"" + String(out) + "\",\"g\":" + String(sda) + "},{\"s\":\"SCL\",\"n\":\"" + String(out) + "\",\"g\":" + String(scl) + "}]}";
    delete m; return true;
  }
  if (ioCount() + m->nsig > IO_MAX) { err = tr("Troppe periferiche (massimo 16)"); delete m; return false; }
  String opt = m->low ? "inv," : ""; if (m->pull == 'u') opt += "up,"; else if (m->pull == 'd') opt += "down,";
  String made[MOD_SIG_MAX]; int nm = 0; bool ok = true;
  String res = "{\"ok\":true,\"devs\":[";
  for (int i = 0; i < m->nsig && ok; i++) {
    String nm1 = prefix.length() ? (m->nsig == 1 ? prefix : prefix + String(i + 1)) : String(), out;
    if (!ioAdd(m->cls, pins[i], nm1, opt, out, err)) { ok = false; break; }
    made[nm++] = out;
    if (i) res += ",";
    res += "{\"s\":\"" + String(m->sig[i]) + "\",\"n\":\"" + out + "\",\"g\":" + String(pins[i]) + "}";
  }
  if (!ok) { String e2; for (int i = 0; i < nm; i++) ioRemove(made[i], e2); delete m; return false; }   // tutto o niente
  vlog("MOD: %s installato (%d periferiche)", m->name.c_str(), m->nsig);
  delete m;
  outJson = res + "]}";
  return true;
}

bool modSave(const String& name, const String& text, String& err) {
  ModInfo* m = new ModInfo();
  bool ok = utilNameOk(name) ? modParse(text, name, *m, err) : (err = tr("Nome non valido"), false);
  delete m;
  if (!ok) return false;
  String e0; fsMkdir(MOD_DIR, e0);
  return fsWriteText(String(MOD_DIR) + "/" + name + MOD_EXT, text, err, false);
}
bool modDelete(const String& name, String& err) {
  if (!utilNameOk(name)) { err = tr("Nome non valido"); return false; }
  return fsRemove(String(MOD_DIR) + "/" + name + MOD_EXT, err);
}

String modListText() {
  String* names = new String[MOD_LIST_MAX];
  int total = scan(names, MOD_LIST_MAX), n = total < MOD_LIST_MAX ? total : MOD_LIST_MAX;
  String s;
  if (!total) s = String(tr("Nessuna scheda modulo in /devs. Si caricano dalla pagina Periferiche")) + "\n";
  else {
    s = String(trf("Schede modulo in /devs: %d", total)) + "\n";
    ModInfo* m = new ModInfo();
    for (int i = 0; i < n; i++) {
      String err;
      if (load(names[i], *m, err)) s += "  " + names[i] + "  " + m->title + "  " + m->cls + " x" + String(m->nsig) + "  OK\n";
      else s += "  " + names[i] + "  " + String(tr("ERRORE")) + ": " + err + "\n";
    }
    delete m;
  }
  delete[] names;
  return s;
}
String modInfoText(const String& name) {
  ModInfo* m = new ModInfo(); String err, s;
  if (!load(name, *m, err)) s = String(tr("ERRORE")) + ": " + err + "\n";
  else {
    uint8_t c = 0; ioParseCls(m->cls, c); int pins[MOD_SIG_MAX]; int k = ioSuggest(c, m->nsig, pins);
    s = m->title + " (" + m->name + ")  v" + m->ver + "\n" + String(tr("Classe del modulo")) + ": " + m->cls + "  " + String(tr("Alimentazione")) + ": " + m->power + (m->low ? "  active=low" : "") + "\n";
    for (int i = 0; i < m->nsig; i++) s += "  " + String(m->sig[i]) + " -> " + (i < k ? "GPIO" + String(pins[i]) : String("?")) + "\n";
    s += String(tr("Livello del segnale")) + ": " + (m->level == '5' ? "5 V" : "3,3 V") + "\n";
    if (m->cls == "i2c") s += String("I2C: ") + m->drv + "  0x" + String(m->addr, HEX) + "  " + (m->fast ? "400 kHz" : "100 kHz") + "\n";
    for (int i = 0; i < m->nwp; i++) s += String(tr("Filo")) + ": " + m->wp[i].lab + " -> " + m->wp[i].dst + "  " + m->wp[i].hex + (m->wp[i].cname[0] ? String(" (") + m->wp[i].cname + ")" : String()) + "\n";
    for (int i = 0; i < m->npart; i++) s += String(tr("Componente")) + ": " + m->part[i].val + " (" + (m->part[i].pos == 's' ? "serie" : m->part[i].pos == 'g' ? "GND" : "VCC") + ") " + m->part[i].sig + "\n";
    if (m->warn.length()) s += String(tr("Avviso del modulo")) + ": " + m->warn + "\n";
    if (k < m->nsig) s += String(tr("Non ci sono abbastanza pin liberi per questo modulo")) + "\n";
  }
  delete m;
  return s;
}
