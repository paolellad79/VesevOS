// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_app.cpp (vedi vos_app.h): lettura e controllo dei file .vap. Nessuna esecuzione in questa versione.
#include "vos_app.h"
#if VOS_WITH_BERRY
#include "vos_berry.h"
#include "vos_app_rt.h"
#include "../core/vos_common.h"
#include "../core/vos_log.h"
#include "../core/vos_i18n.h"
#include "../core/vos_files.h"
#include "../core/vos_util.h"


static bool nameChars(const String& n, bool allowDash) {
  if (n.length() < 1 || n.length() > 24) return false;
  for (size_t i = 0; i < n.length(); i++) {
    char c = n[i];
    if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || (allowDash && c == '-'))) return false;
  }
  return true;
}
bool appNameValid(const String& n) { return nameChars(n, true); }
static bool idOk(const String& s) { return s.length() >= 1 && s.length() <= 12 && nameChars(s, false); }
static bool txtOk(const String& s, size_t mx) {                  // 1..mx caratteri, niente caratteri di controllo
  if (s.length() < 1 || s.length() > mx) return false;
  for (size_t i = 0; i < s.length(); i++) { unsigned char c = (unsigned char)s[i]; if (c < 32 || c == 127) return false; }
  return true;
}
static bool intOk(const String& s, long& v) {
  size_t i = 0; if (s.length() && s[0] == '-') i = 1;
  if (i >= s.length() || s.length() - i > 6) return false;
  for (size_t k = i; k < s.length(); k++) if (s[k] < '0' || s[k] > '9') return false;
  v = s.toInt(); return true;
}

// ---------- icona SVG: solo forme semplici (controllo in vos_util: utilSvgSafe) ----------
bool appIconSafe(const String& svg) { return utilSvgSafe(svg, APP_ICON_MAX); }

// ---------- una riga di [page]: tipo id=.. label=".." [min= max= points=] ----------
static bool pageLine(const String& t, AppCtl& c, String& why) {
  memset(&c, 0, sizeof(c));
  size_t p = 0, n = t.length();
  String kind;
  while (p < n && t[p] != ' ' && t[p] != '\t') kind += t[p++];
  if (kind == "switch") c.k = 's'; else if (kind == "slider") c.k = 'l'; else if (kind == "color") c.k = 'c';
  else if (kind == "text") c.k = 't'; else if (kind == "button") c.k = 'b'; else if (kind == "chart") c.k = 'g';
  else { why = "k"; return false; }
  c.mn = 0; c.mx = 100; c.pts = 60;
  bool hasId = false, hasLabel = false, hasMn = false, hasMx = false, hasPts = false;
  while (true) {
    while (p < n && (t[p] == ' ' || t[p] == '\t')) p++;
    if (p >= n) break;
    String key, val;
    while (p < n && t[p] != '=' && t[p] != ' ' && t[p] != '\t') key += t[p++];
    if (p >= n || t[p] != '=') { why = "p"; return false; }
    p++;
    if (p < n && t[p] == '"') { p++; while (p < n && t[p] != '"') val += t[p++]; if (p >= n) { why = "p"; return false; } p++; }
    else { while (p < n && t[p] != ' ' && t[p] != '\t') val += t[p++]; }
    long v;
    if (key == "id") { if (hasId || !idOk(val)) { why = "i"; return false; } strncpy(c.id, val.c_str(), 12); c.id[12] = 0; hasId = true; }
    else if (key == "label") { if (hasLabel || !txtOk(val, 24)) { why = "t"; return false; } strncpy(c.label, val.c_str(), 24); c.label[24] = 0; hasLabel = true; }
    else if (key == "min" && c.k == 'l') { if (hasMn || !intOk(val, v) || v < -100000 || v > 100000) { why = "p"; return false; } c.mn = v; hasMn = true; }
    else if (key == "max" && c.k == 'l') { if (hasMx || !intOk(val, v) || v < -100000 || v > 100000) { why = "p"; return false; } c.mx = v; hasMx = true; }
    else if (key == "points" && c.k == 'g') { if (hasPts || !intOk(val, v) || v < 10 || v > 120) { why = "p"; return false; } c.pts = (uint16_t)v; hasPts = true; }
    else { why = "p"; return false; }
  }
  if (!hasId) { why = "i"; return false; }
  if (!hasLabel) { why = "t"; return false; }
  if (c.k == 'l' && c.mn >= c.mx) { why = "p"; return false; }
  return true;
}

static bool usePermOk(const String& u, String& norm) {
  norm = "";
  int cnt = 0, pos = 0;
  while (pos <= (int)u.length()) {
    int e = u.indexOf(',', pos); if (e < 0) e = u.length();
    String p = u.substring(pos, e); p.trim(); pos = e + 1;
    if (p.length() == 0) { if (u.length() == 0) break; return false; }
    bool ok = (p == "led" || p == "time" || p == "temp" || p == "log" || p == "act" || p == "btn" || p == "mqtt" || p == "logread");
    if (!ok && p.length() >= 2 && p.length() <= 11 && !p.startsWith("gpio")) {      // nome di una periferica a classi (rele1, led1...)
      ok = true;
      for (size_t k = 0; k < p.length() && ok; k++) { char ch = p[k]; ok = (ch >= 'a' && ch <= 'z') || (k > 0 && ch >= '0' && ch <= '9'); }
    }
    if (!ok && p.startsWith("gpio:")) {
      String g = p.substring(5); long v;
      ok = g.length() <= 2 && intOk(g, v) && v >= 0 && v <= VOS_GPIO_MAX;
      if (ok) p = String("gpio:") + String(v);
    }
    if (!ok || ++cnt > 8) return false;
    if (norm.length()) norm += ",";
    norm += p;
  }
  return true;
}

bool appParse(const String& text, const String& fileName, AppInfo& a, String& err) {
  a.name = ""; a.title = ""; a.ver = "1"; a.author = ""; a.who = "admin"; a.use = ""; a.icon = ""; a.code = ""; a.data = ""; a.nctl = 0; a.ndata = 0; a.size = text.length(); a.codeLen = 0;
  err = "";
  if (text.length() > APP_FILE_MAX) { err = tr("File troppo grande (massimo 16 KB)"); return false; }
  enum { NONE, MAN, ICO, PAG, COD, DAT } sec = NONE;
  bool seen[6] = {false, false, false, false, false, false};
  String code;
  String dataKeys;
  size_t pos = 0; int line = 0;
  while (pos <= text.length()) {
    int e = text.indexOf('\n', pos); if (e < 0) e = text.length();
    String raw = text.substring(pos, e); pos = e + 1; line++;
    if (raw.endsWith("\r")) raw.remove(raw.length() - 1);
    String t = raw; t.trim();
    if (t.length() > 1 && t[0] == '[' && t[t.length() - 1] == ']') {
      String s = t.substring(1, t.length() - 1); s.toLowerCase();
      int ns = s == "manifest" ? MAN : s == "icon" ? ICO : s == "page" ? PAG : s == "code" ? COD : s == "data" ? DAT : NONE;
      if (ns != NONE) {
        if (seen[ns]) return utilLineErr(err, line, tr("Sezione ripetuta"));
        seen[ns] = true; sec = (decltype(sec))ns; continue;
      }
      if (sec != COD) return utilLineErr(err, line, tr("Sezione sconosciuta (ammesse: manifest, icon, page, code, data)"));
    }
    if (sec == COD) { code += raw; code += "\n"; if (code.length() > APP_CODE_MAX + 8) return utilLineErr(err, line, tr("Codice troppo lungo (massimo 8 KB)")); continue; }
    if (t.length() == 0 || t[0] == '#') continue;
    if (sec == NONE) return utilLineErr(err, line, tr("Riga fuori da una sezione"));
    if (sec == ICO) { a.icon += t; a.icon += "\n"; if (a.icon.length() > APP_ICON_MAX + 8) return utilLineErr(err, line, tr("Icona non ammessa (un solo svg con forme semplici, massimo 2 KB)")); continue; }
    if (sec == PAG) {
      if (a.nctl >= APP_CTL_MAX) return utilLineErr(err, line, tr("Troppi controlli (massimo 12)"));
      String why;
      if (!pageLine(t, a.ctl[a.nctl], why)) {
        if (why == "k") return utilLineErr(err, line, tr("Controllo non valido (switch, slider, color, text, button, chart)"));
        if (why == "i") return utilLineErr(err, line, tr("Serve id (a-z, 0-9, _ massimo 12)"));
        if (why == "t") return utilLineErr(err, line, tr("Testo non valido (1-24 caratteri, niente caratteri di controllo)"));
        return utilLineErr(err, line, tr("Parametro non valido per questo controllo"));
      }
      for (int i = 0; i < a.nctl; i++) if (strcmp(a.ctl[i].id, a.ctl[a.nctl].id) == 0) return utilLineErr(err, line, tr("Id ripetuto"));
      a.nctl++; continue;
    }
    // manifest e data: chiave=valore
    int q = t.indexOf('=');
    if (q < 1) return utilLineErr(err, line, tr("Serve chiave=valore"));
    String k = t.substring(0, q), v = t.substring(q + 1); k.trim(); v.trim(); k.toLowerCase();
    if (sec == DAT) {
      if (!idOk(k) || !txtOk(v, 40)) return utilLineErr(err, line, tr("Dato non valido (chiave a-z 0-9 _, valore massimo 40 caratteri)"));
      if (dataKeys.indexOf("," + k + ",") >= 0) return utilLineErr(err, line, tr("Id ripetuto"));
      if (++a.ndata > APP_DATA_MAX) return utilLineErr(err, line, tr("Troppi dati (massimo 12)"));
      dataKeys += "," + k + ","; a.data += k + "=" + v + "\n";
      continue;
    }
    if (k == "name") { if (!appNameValid(v) || v != fileName) return utilLineErr(err, line, tr("Nome non valido o diverso dal nome del file")); a.name = v; }
    else if (k == "title") { if (!txtOk(v, 24)) return utilLineErr(err, line, tr("Testo non valido (1-24 caratteri, niente caratteri di controllo)")); a.title = v; }
    else if (k == "author") { if (!txtOk(v, 24)) return utilLineErr(err, line, tr("Testo non valido (1-24 caratteri, niente caratteri di controllo)")); a.author = v; }
    else if (k == "version") {
      bool ok = v.length() >= 1 && v.length() <= 10;
      for (size_t i = 0; ok && i < v.length(); i++) if (!((v[i] >= '0' && v[i] <= '9') || v[i] == '.')) ok = false;
      if (!ok) return utilLineErr(err, line, tr("Versione non valida (solo cifre e punti, massimo 10)"));
      a.ver = v;
    }
    else if (k == "who") { if (v != "admin" && v != "oper") return utilLineErr(err, line, tr("Chi la usa: admin oppure oper")); a.who = v; }
    else if (k == "use") { String norm; if (!usePermOk(v, norm)) return utilLineErr(err, line, tr("Permesso non ammesso (led, time, temp, log, logread, act, btn, mqtt, gpio:N o il nome di una periferica come rele1)")); a.use = norm; }
    else return utilLineErr(err, line, tr("Chiave sconosciuta nel manifest"));
  }
  if (!seen[MAN] || a.name.length() == 0) { err = tr("Manca [manifest] con name="); return false; }
  if (a.title.length() == 0) a.title = a.name;
  String c2 = code; c2.trim();
  if (!seen[COD] || c2.length() == 0) { err = tr("Manca la sezione [code]"); return false; }
  a.codeLen = c2.length(); a.code = c2;
  if (a.codeLen > APP_CODE_MAX) { err = tr("Codice troppo lungo (massimo 8 KB)"); return false; }
  if (seen[ICO]) {
    a.icon.trim();
    if (!appIconSafe(a.icon)) { err = tr("Icona non ammessa (un solo svg con forme semplici, massimo 2 KB)"); return false; }
  } else a.icon = "";
  return true;
}

// ---------- elenco dei file /apps/*.vap ----------
static int scanApps(void (*cb)(const String&, void*), void* ctx) {
  String j = fsListJson(APP_DIR);
  if (j.indexOf("\"ok\":true") < 0) { String e; fsMkdir(APP_DIR, e); j = fsListJson(APP_DIR); }
  int n = 0, pos = 0;
  while (true) {
    int a = j.indexOf("{\"n\":\"", pos); if (a < 0) break;
    int b = j.indexOf("\"", a + 6); if (b < 0) break;
    String name = j.substring(a + 6, b);
    int d = j.indexOf("\"d\":", b);
    bool isDir = d >= 0 && j.substring(d + 4, d + 8) == "true";
    pos = b + 1;
    if (isDir || !name.endsWith(APP_EXT)) continue;
    cb(name.substring(0, name.length() - 4), ctx); n++;
  }
  return n;
}

static bool loadApp(const String& name, AppInfo& a, String& err) {
  String text, e;
  if (!appNameValid(name)) { err = tr("Nome app non valido"); return false; }
  if (!fsReadText(String(APP_DIR) + "/" + name + APP_EXT, text, e)) { err = e.length() ? e : String(tr("File non trovato")); return false; }
  return appParse(text, name, a, err);
}

static const char* kindName(char k) {
  switch (k) { case 's': return "switch"; case 'l': return "slider"; case 'c': return "color"; case 't': return "text"; case 'b': return "button"; default: return "chart"; }
}

struct NamesCtx { String names[APP_CTL_MAX]; int n; int total; };
static void namesCb(const String& nm, void* c) { NamesCtx* x = (NamesCtx*)c; x->total++; if (x->n < APP_CTL_MAX) x->names[x->n++] = nm; }

String appListText() {
  NamesCtx* x = new NamesCtx(); x->n = 0; x->total = 0;
  scanApps(namesCb, x);
  String s;
  if (!x->total) s = String(tr("Nessuna app in /apps. Si crea dalla pagina App (Programmi > App)")) + "\n";
  else {
    s = String(trf("App in /apps: %d", x->total)) + "\n";
    AppInfo* a = new AppInfo();
    for (int i = 0; i < x->n; i++) {
      String err;
      if (loadApp(x->names[i], *a, err)) s += "  " + x->names[i] + "  " + a->title + "  v" + a->ver + "  OK\n";
      else s += "  " + x->names[i] + "  " + String(tr("ERRORE")) + ": " + err + "\n";
    }
    delete a;
  }
  delete x;
  return s;
}

String appInfoText(const String& nameIn) {
  String n = nameIn; n.trim(); n.toLowerCase();
  if (n.endsWith(APP_EXT)) n.remove(n.length() - 4);
  if (!appNameValid(n)) return String(tr("Nome app non valido")) + "\n";
  AppInfo* a = new AppInfo(); String err, s;
  if (!loadApp(n, *a, err)) s = String(tr("ERRORE")) + ": " + err + "\n";
  else {
    s = a->title + " (" + a->name + ") v" + a->ver + (a->author.length() ? " - " + a->author : String("")) + "\n";
    s += String(trf("Permessi: %s", a->use.length() ? a->use.c_str() : "-")) + "\n";
    s += String(trf("Chi la usa: %s", a->who.c_str())) + "\n";
    s += String(trf("Controlli: %d", a->nctl)) + "\n";
    for (int i = 0; i < a->nctl; i++) s += String("  ") + kindName(a->ctl[i].k) + " " + a->ctl[i].id + "  " + a->ctl[i].label + "\n";
    s += String(trf("Dati iniziali: %d", a->ndata)) + "\n";
    s += String(trf("Codice: %d byte, file: %d byte", (int)a->codeLen, (int)a->size)) + "\n";
    s += String(a->icon.length() ? tr("Icona: si") : tr("Icona: no")) + "\n";
    s += String(tr("File valido. Si avvia con: app start <nome>")) + "\n";
  }
  delete a;
  return s;
}

static bool roleCan(const AppInfo& a, uint8_t role) { return role >= 2 || a.who == "oper"; }   // l'operatore usa solo le app con who=oper

String appListJson(uint8_t role) {
  NamesCtx* x = new NamesCtx(); x->n = 0; x->total = 0;
  scanApps(namesCb, x);
  String j = "{\"ok\":true,\"max\":" + String((unsigned)APP_FILE_MAX) + ",\"more\":" + (x->total > x->n ? "true" : "false") + ",\"list\":[";
  AppInfo* a = new AppInfo();
  bool first = true;
  for (int i = 0; i < x->n; i++) {
    String err;
    bool okl = loadApp(x->names[i], *a, err);
    if (role < 2 && (!okl || !roleCan(*a, role))) continue;     // l'operatore non vede gli errori ne le app solo per Admin
    if (!first) j += ","; first = false;
    if (okl)
      j += "{\"n\":\"" + a->name + "\",\"ok\":true,\"t\":\"" + jsonEscape(a->title) + "\",\"v\":\"" + a->ver + "\",\"a\":\"" + jsonEscape(a->author) +
           "\",\"w\":\"" + a->who + "\",\"u\":\"" + a->use + "\",\"c\":" + String(a->nctl) + ",\"s\":" + String((unsigned)a->size) + ",\"icon\":\"" + jsonEscape(a->icon) + "\"}";
    else
      j += "{\"n\":\"" + jsonEscape(x->names[i]) + "\",\"ok\":false,\"e\":\"" + jsonEscape(err) + "\"}";
  }
  delete a; delete x;
  return j + "]}";
}

String appInfoJson(const String& nameIn, uint8_t role) {
  String n = nameIn; n.trim(); n.toLowerCase();
  if (n.endsWith(APP_EXT)) n.remove(n.length() - 4);
  if (!appNameValid(n)) return "{\"ok\":false,\"err\":\"" + jsonEscape(String(tr("Nome app non valido"))) + "\",\"code\":\"error\"}";
  AppInfo* a = new AppInfo(); String err, j;
  if (!loadApp(n, *a, err)) j = "{\"ok\":false,\"err\":\"" + jsonEscape(err) + "\",\"code\":\"error\"}";
  else if (!roleCan(*a, role)) j = "{\"ok\":false,\"err\":\"" + jsonEscape(String(tr("Il tuo ruolo non permette questa operazione"))) + "\",\"code\":\"forbidden\"}";
  else {
    j = "{\"ok\":true,\"n\":\"" + a->name + "\",\"t\":\"" + jsonEscape(a->title) + "\",\"v\":\"" + a->ver + "\",\"a\":\"" + jsonEscape(a->author) + "\",\"w\":\"" + a->who +
        "\",\"u\":\"" + a->use + "\",\"s\":" + String((unsigned)a->size) + ",\"code\":" + String((unsigned)a->codeLen) + ",\"data\":" + String(a->ndata) +
        ",\"icon\":\"" + jsonEscape(a->icon) + "\",\"ctl\":[";
    for (int i = 0; i < a->nctl; i++) {
      if (i) j += ",";
      j += String("{\"k\":\"") + kindName(a->ctl[i].k) + "\",\"id\":\"" + a->ctl[i].id + "\",\"l\":\"" + jsonEscape(String(a->ctl[i].label)) + "\"";
      if (a->ctl[i].k == 'l') j += ",\"mn\":" + String((long)a->ctl[i].mn) + ",\"mx\":" + String((long)a->ctl[i].mx);
      if (a->ctl[i].k == 'g') j += ",\"p\":" + String((int)a->ctl[i].pts);
      j += "}";
    }
    j += "]}";
  }
  delete a;
  return j;
}

// ---------- esecuzione ----------
static String cleanAppName(const String& in) { String n = in; n.trim(); n.toLowerCase(); if (n.endsWith(APP_EXT)) n.remove(n.length() - 4); return n; }

bool appStart(const String& nameIn, String& err, uint8_t role, const char* who) {
  err = "";
  String n = cleanAppName(nameIn);
  AppInfo* a = new AppInfo();
  if (!loadApp(n, *a, err)) { delete a; return false; }
  if (!roleCan(*a, role)) { err = tr("Il tuo ruolo non permette questa operazione"); delete a; return false; }
  String cur;
  if (berryAppRunning(cur)) { err = (cur == n) ? String(tr("App gia in esecuzione")) : String(tr("Berry e occupato con un altro script")); delete a; return false; }
  if (berryRunning()) { err = tr("Berry e occupato con un altro script"); delete a; return false; }
  rtInit(*a);
  if (!berryStartApp(a->name, a->code, a->use, err)) { rtFree(); delete a; return false; }
  vlog("APP: %s avviata da %s", a->name.c_str(), who ? who : "?");
  delete a;
  return true;
}

bool appStop(const String& nameIn, String& err, uint8_t role) {
  err = "";
  String n = cleanAppName(nameIn), cur;
  if (!appNameValid(n)) { err = tr("Nome app non valido"); return false; }
  if (!berryAppRunning(cur) || cur != n) { err = tr("L'app non e in esecuzione"); return false; }
  AppInfo* a = new AppInfo(); String e2;
  bool okl = loadApp(n, *a, e2); bool can = !okl || roleCan(*a, role);
  delete a;
  if (!can) { err = tr("Il tuo ruolo non permette questa operazione"); return false; }
  if (!berryStop(err)) return false;
  vlog("APP: %s fermata", n.c_str());
  return true;
}

bool appSet(const String& nameIn, const String& id, const String& value, String& err, uint8_t role) {
  err = "";
  String n = cleanAppName(nameIn), cur;
  if (!appNameValid(n)) { err = tr("Nome app non valido"); return false; }
  AppInfo* a = new AppInfo(); String e2;
  bool okl = loadApp(n, *a, e2); bool can = okl && roleCan(*a, role);
  delete a;
  if (!okl) { err = e2; return false; }
  if (!can) { err = tr("Il tuo ruolo non permette questa operazione"); return false; }
  if (!berryAppRunning(cur) || cur != n) { err = tr("L'app non e in esecuzione"); return false; }
  return rtSetWeb(id, value, err);
}

String appStateJson(const String& nameIn, uint8_t role) {
  String n = cleanAppName(nameIn);
  if (!appNameValid(n)) return "{\"ok\":false,\"err\":\"" + jsonEscape(String(tr("Nome app non valido"))) + "\",\"code\":\"error\"}";
  AppInfo* a = new AppInfo(); String err;
  bool okl = loadApp(n, *a, err); bool can = okl && roleCan(*a, role);
  delete a;
  if (!okl) return "{\"ok\":false,\"err\":\"" + jsonEscape(err) + "\",\"code\":\"error\"}";
  if (!can) return "{\"ok\":false,\"err\":\"" + jsonEscape(String(tr("Il tuo ruolo non permette questa operazione"))) + "\",\"code\":\"forbidden\"}";
  String cur; bool run = berryAppRunning(cur) && cur == n;
  bool mine = berryLastIsApp(n);
  String j = String("{\"ok\":true,\"n\":\"") + n + "\",\"run\":" + (run ? "true" : "false") + ",\"busy\":" + (berryRunning() ? "true" : "false") + ",";
  j += (mine ? berryAppOutJson() : String("\"out\":\"\",\"err\":\"\",\"ms\":0,\"last\":-1"));
  j += ",\"vals\":" + ((rtLoaded() && rtName() == n) ? rtValuesJson() : String("[]")) + "}";
  return j;
}

String appRunText() { return berryOutText(); }
#endif
