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
#include "../core/vos_vdesc.h"

// ---------- controlli sui testi (viste: nessuna copia) ----------
static bool nameChars(std::string_view n, bool allowDash) {
  if (n.size() < 1 || n.size() > 24) return false;
  for (char c : n) {
    if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || (allowDash && c == '-'))) return false;
  }
  return true;
}
bool appNameValid(std::string_view n) { return nameChars(n, true); }
static bool idOk(std::string_view s) { return s.size() >= 1 && s.size() <= 12 && nameChars(s, false); }
static bool txtOk(std::string_view s, size_t mx) {                  // 1..mx caratteri, niente caratteri di controllo
  if (s.size() < 1 || s.size() > mx) return false;
  for (char ch : s) { unsigned char c = (unsigned char)ch; if (c < 32 || c == 127) return false; }
  return true;
}
static bool intOk(std::string_view s, long& v) {
  size_t i = (!s.empty() && s[0] == '-') ? 1 : 0;
  if (i >= s.size() || s.size() - i > 6) return false;
  for (size_t k = i; k < s.size(); k++) if (s[k] < '0' || s[k] > '9') return false;
  v = viewToLong(s); return true;
}

// ---------- icona SVG: solo forme semplici (controllo in vos_util: utilSvgSafe) ----------
bool appIconSafe(std::string_view svg) { return utilSvgSafe(svg, APP_ICON_MAX); }

// ---------- una riga di [page]: tipo id=.. label=".." [min= max= points=] ----------
// why: "k" tipo sconosciuto, "p" parametro, "i" id, "t" testo
static bool pageLine(std::string_view t, AppCtl& c, const char*& why) {
  memset(&c, 0, sizeof(c));
  size_t p = 0, n = t.size();
  while (p < n && t[p] != ' ' && t[p] != '\t') p++;
  std::string_view kind = t.substr(0, p);
  if (kind == "switch") c.k = 's'; else if (kind == "slider") c.k = 'l'; else if (kind == "color") c.k = 'c';
  else if (kind == "text") c.k = 't'; else if (kind == "button") c.k = 'b'; else if (kind == "chart") c.k = 'g';
  else { why = "k"; return false; }
  c.mn = 0; c.mx = 100; c.pts = 60;
  bool hasId = false, hasLabel = false, hasMn = false, hasMx = false, hasPts = false;
  while (true) {
    while (p < n && (t[p] == ' ' || t[p] == '\t')) p++;
    if (p >= n) break;
    size_t k0 = p;
    while (p < n && t[p] != '=' && t[p] != ' ' && t[p] != '\t') p++;
    std::string_view key = t.substr(k0, p - k0), val;
    if (p >= n || t[p] != '=') { why = "p"; return false; }
    p++;
    if (p < n && t[p] == '"') {
      p++; size_t v0 = p;
      while (p < n && t[p] != '"') p++;
      if (p >= n) { why = "p"; return false; }
      val = t.substr(v0, p - v0); p++;
    } else {
      size_t v0 = p;
      while (p < n && t[p] != ' ' && t[p] != '\t') p++;
      val = t.substr(v0, p - v0);
    }
    long v;
    if (key == "id") { if (hasId || !idOk(val)) { why = "i"; return false; } viewCopy(c.id, sizeof c.id, val); hasId = true; }
    else if (key == "label") { if (hasLabel || !txtOk(val, 24)) { why = "t"; return false; } viewCopy(c.label, sizeof c.label, val); hasLabel = true; }
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

// Permessi del manifest (use=): elenco separato da virgole, normalizzato in norm (cap byte). false = non ammesso.
static bool usePermOk(std::string_view u, char* norm, size_t cap) {
  norm[0] = 0;
  size_t nlen = 0;
  int cnt = 0;
  size_t pos = 0;
  while (pos <= u.size()) {
    size_t e = u.find(',', pos); if (e == std::string_view::npos) e = u.size();
    std::string_view p = trimView(u.substr(pos, e - pos));
    pos = e + 1;
    if (p.empty()) { if (u.empty()) break; return false; }
    char gp[16];                                                     // "gpio:N" riscritto senza zeri davanti
    bool ok = (p == "led" || p == "time" || p == "temp" || p == "log" || p == "act" || p == "btn" || p == "mqtt" || p == "logread" || p == "ble");
    bool isGpio = p.substr(0, 4) == "gpio";
    if (!ok && p.size() >= 2 && p.size() <= 11 && !isGpio) {          // nome di una periferica a classi (rele1, led1...)
      ok = true;
      for (size_t k = 0; k < p.size() && ok; k++) { char ch = p[k]; ok = (ch >= 'a' && ch <= 'z') || (k > 0 && ch >= '0' && ch <= '9'); }
    }
    if (!ok && p.substr(0, 5) == "gpio:") {
      std::string_view g = p.substr(5); long v;
      ok = g.size() <= 2 && intOk(g, v) && v >= 0 && v <= VOS_GPIO_MAX;
      if (ok) { snprintf(gp, sizeof gp, "gpio:%ld", v); p = gp; }
    }
    if (!ok || ++cnt > 8) return false;
    if (nlen && !bufAppend(norm, cap, nlen, ",")) return false;
    if (!bufAppend(norm, cap, nlen, p)) return false;
  }
  return true;
}

// true se nel testo [data] (righe chiave=valore) c'e gia la chiave
static bool dataHasKey(const char* data, std::string_view key) {
  LineIter it{std::string_view(data)};
  std::string_view ln;
  while (it.next(ln)) {
    size_t q = ln.find('=');
    if (q != std::string_view::npos && ln.substr(0, q) == key) return true;
  }
  return false;
}

// Toglie gli spazi ai lati di un testo in un buffer (come trim di Arduino). Ritorna la nuova lunghezza.
static size_t trimBuf(char* b) {
  std::string_view t = trimView(std::string_view(b));
  memmove(b, t.data(), t.size());
  b[t.size()] = 0;
  return t.size();
}

bool appParse(std::string_view text, const char* fileName, AppInfo& a, ErrBuf& err) {
  memset(&a, 0, sizeof(a));
  strcpy(a.ver, "1"); strcpy(a.who, "admin");
  a.size = text.size();
  err = "";
  if (text.size() > APP_FILE_MAX) { err = tr("File troppo grande (massimo 16 KB)"); return false; }
  enum { NONE, MAN, ICO, PAG, COD, DAT } sec = NONE;
  bool seen[6] = {false, false, false, false, false, false};
  size_t codeLen = 0, iconLen = 0;                                   // a.code e a.icon si riempiono riga per riga
  LineIter lines(text);
  std::string_view raw;
  while (lines.next(raw)) {
    int line = lines.line();
    std::string_view t = trimView(raw);
    if (t.size() > 1 && t[0] == '[' && t.back() == ']') {
      std::string_view s = t.substr(1, t.size() - 2);
      int ns = viewEqI(s, "manifest") ? MAN : viewEqI(s, "icon") ? ICO : viewEqI(s, "page") ? PAG : viewEqI(s, "code") ? COD : viewEqI(s, "data") ? DAT : NONE;
      if (ns != NONE) {
        if (seen[ns]) return utilLineErr(err, line, tr("Sezione ripetuta"));
        seen[ns] = true; sec = (decltype(sec))ns; continue;
      }
      if (sec != COD) return utilLineErr(err, line, tr("Sezione sconosciuta (ammesse: manifest, icon, page, code, data)"));
    }
    if (sec == COD) {
      if (codeLen + raw.size() + 1 > APP_CODE_MAX + 8) return utilLineErr(err, line, tr("Codice troppo lungo (massimo 8 KB)"));
      bufAppend(a.code, sizeof a.code, codeLen, raw); bufAppend(a.code, sizeof a.code, codeLen, "\n");
      continue;
    }
    if (t.empty() || t[0] == '#') continue;
    if (sec == NONE) return utilLineErr(err, line, tr("Riga fuori da una sezione"));
    if (sec == ICO) {
      if (iconLen + t.size() + 1 > APP_ICON_MAX + 8) return utilLineErr(err, line, tr("Icona non ammessa (un solo svg con forme semplici, massimo 2 KB)"));
      bufAppend(a.icon, sizeof a.icon, iconLen, t); bufAppend(a.icon, sizeof a.icon, iconLen, "\n");
      continue;
    }
    if (sec == PAG) {
      if (a.nctl >= APP_CTL_MAX) return utilLineErr(err, line, tr("Troppi controlli (massimo 12)"));
      const char* why = "p";
      if (!pageLine(t, a.ctl[a.nctl], why)) {
        if (!strcmp(why, "k")) return utilLineErr(err, line, tr("Controllo non valido (switch, slider, color, text, button, chart)"));
        if (!strcmp(why, "i")) return utilLineErr(err, line, tr("Serve id (a-z, 0-9, _ massimo 12)"));
        if (!strcmp(why, "t")) return utilLineErr(err, line, tr("Testo non valido (1-24 caratteri, niente caratteri di controllo)"));
        return utilLineErr(err, line, tr("Parametro non valido per questo controllo"));
      }
      for (int i = 0; i < a.nctl; i++) if (strcmp(a.ctl[i].id, a.ctl[a.nctl].id) == 0) return utilLineErr(err, line, tr("Id ripetuto"));
      a.nctl++; continue;
    }
    // manifest e data: chiave=valore
    size_t q = t.find('=');
    if (q == std::string_view::npos || q < 1) return utilLineErr(err, line, tr("Serve chiave=valore"));
    std::string_view k = trimView(t.substr(0, q)), v = trimView(t.substr(q + 1));
    if (sec == DAT) {
      char kb[13];                                                   // la chiave si salva in minuscolo
      if (!viewCopyLower(kb, sizeof kb, k) || !idOk(kb) || !txtOk(v, 40)) return utilLineErr(err, line, tr("Dato non valido (chiave a-z 0-9 _, valore massimo 40 caratteri)"));
      if (dataHasKey(a.data, kb)) return utilLineErr(err, line, tr("Id ripetuto"));
      if (++a.ndata > APP_DATA_MAX) return utilLineErr(err, line, tr("Troppi dati (massimo 12)"));
      size_t dl = strlen(a.data);
      bufAppend(a.data, sizeof a.data, dl, kb); bufAppend(a.data, sizeof a.data, dl, "="); bufAppend(a.data, sizeof a.data, dl, v); bufAppend(a.data, sizeof a.data, dl, "\n");
      continue;
    }
    if (viewEqI(k, "name")) { if (!appNameValid(v) || v != std::string_view(fileName)) return utilLineErr(err, line, tr("Nome non valido o diverso dal nome del file")); viewCopy(a.name, sizeof a.name, v); }
    else if (viewEqI(k, "title")) { if (!txtOk(v, 24)) return utilLineErr(err, line, tr("Testo non valido (1-24 caratteri, niente caratteri di controllo)")); viewCopy(a.title, sizeof a.title, v); }
    else if (viewEqI(k, "author")) { if (!txtOk(v, 24)) return utilLineErr(err, line, tr("Testo non valido (1-24 caratteri, niente caratteri di controllo)")); viewCopy(a.author, sizeof a.author, v); }
    else if (viewEqI(k, "version")) {
      bool ok = v.size() >= 1 && v.size() <= 10;
      for (size_t i = 0; ok && i < v.size(); i++) if (!((v[i] >= '0' && v[i] <= '9') || v[i] == '.')) ok = false;
      if (!ok) return utilLineErr(err, line, tr("Versione non valida (solo cifre e punti, massimo 10)"));
      viewCopy(a.ver, sizeof a.ver, v);
    }
    else if (viewEqI(k, "who")) { if (v != "admin" && v != "oper") return utilLineErr(err, line, tr("Chi la usa: admin oppure oper")); viewCopy(a.who, sizeof a.who, v); }
    else if (viewEqI(k, "use")) { if (!usePermOk(v, a.use, sizeof a.use)) return utilLineErr(err, line, tr("Permesso non ammesso (led, time, temp, log, logread, act, btn, mqtt, ble, gpio:N o il nome di una periferica come rele1)")); }
    else return utilLineErr(err, line, tr("Chiave sconosciuta nel manifest"));
  }
  if (!seen[MAN] || a.name[0] == 0) { err = tr("Manca [manifest] con name="); return false; }
  if (a.title[0] == 0) strcpy(a.title, a.name);
  a.codeLen = trimBuf(a.code);
  if (!seen[COD] || a.codeLen == 0) { err = tr("Manca la sezione [code]"); return false; }
  if (a.codeLen > APP_CODE_MAX) { err = tr("Codice troppo lungo (massimo 8 KB)"); return false; }
  if (seen[ICO]) {
    trimBuf(a.icon);
    if (!appIconSafe(a.icon)) { err = tr("Icona non ammessa (un solo svg con forme semplici, massimo 2 KB)"); return false; }
  } else a.icon[0] = 0;
  return true;
}

// ---------- elenco dei file /apps/*.vap ----------
// Chiama cb(nome senza estensione, ctx) per ogni file .vap della cartella (la crea se manca). Ritorna quanti ne ha trovati.
typedef void (*NameCb)(std::string_view, void*);
static int scanApps(NameCb cb, void* ctx) {
  HeapBuf j(FS_JSON_MAX);
  if (!fsListBuild(APP_DIR, j)) return 0;                          // manca la memoria
  if (!strstr(j.c_str(), "\"ok\":true")) { ErrBuf e; fsMkdir(APP_DIR, e); j.clear(); if (!fsListBuild(APP_DIR, j)) return 0; }
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
    if (isDir || name.size() < 4 || name.substr(name.size() - 4) != APP_EXT) continue;
    cb(name.substr(0, name.size() - 4), ctx); n++;
  }
  return n;
}

static bool loadApp(const char* name, AppInfo& a, ErrBuf& err) {
  HeapBuf text(fsTextMax()); ErrBuf e;
  if (!appNameValid(name)) { err = tr("Nome app non valido"); return false; }
  char path[64];
  snprintf(path, sizeof path, "%s/%s%s", APP_DIR, name, APP_EXT);   // il nome e gia valido: al massimo 24 caratteri
  if (!fsReadText(path, text, e)) { err = e.empty() ? tr("File non trovato") : e.c_str(); return false; }
  return appParse(std::string_view(text.c_str(), text.length()), name, a, err);
}

static const char* kindName(char k) {
  switch (k) { case 's': return "switch"; case 'l': return "slider"; case 'c': return "color"; case 't': return "text"; case 'b': return "button"; default: return "chart"; }
}

// Primi nomi dell'elenco (i file piu lunghi si tagliano: restano comunque nomi non validi, e si mostrano come errore)
#define APP_NAME_BUF 48
struct NamesCtx { char names[APP_CTL_MAX][APP_NAME_BUF]; int n; int total; };
static void namesCb(std::string_view nm, void* c) {
  NamesCtx* x = (NamesCtx*)c; x->total++;
  if (x->n >= APP_CTL_MAX) return;
  size_t k = nm.size() < APP_NAME_BUF - 1 ? nm.size() : APP_NAME_BUF - 1;
  memcpy(x->names[x->n], nm.data(), k); x->names[x->n][k] = 0; x->n++;
}

static bool full(const HeapBuf& o) { return o.oom() || o.over(); }

// Nome scritto dall'utente -> nome pulito: senza spazi ai lati, minuscolo, senza ".vap". Se e troppo lungo per essere un nome, out resta vuoto
// (e vuoto non e un nome valido).
static void cleanAppName(const char* in, char* out, size_t cap) {
  std::string_view v = trimView(std::string_view(in ? in : ""));
  if (!viewCopyLower(out, cap, v)) return;
  size_t n = v.size(), e = strlen(APP_EXT);
  if (n >= e && !strcmp(out + n - e, APP_EXT)) out[n - e] = 0;
}
#define APP_CLEAN_BUF 64

void appListText(HeapBuf& out) {
  NamesCtx* x = new NamesCtx(); x->n = 0; x->total = 0;
  scanApps(namesCb, x);
  if (!x->total) { out.add(tr("Nessuna app in /apps. Si crea dalla pagina App (Programmi > App)")); out.add('\n'); }
  else {
    heapAddf(out, tr("App in /apps: %d"), x->total); out.add('\n');
    AppInfo* a = new AppInfo();
    for (int i = 0; i < x->n; i++) {
      ErrBuf err;
      if (loadApp(x->names[i], *a, err)) heapAddf(out, "  %s  %s  v%s  OK\n", x->names[i], a->title, a->ver);
      else heapAddf(out, "  %s  %s: %s\n", x->names[i], tr("ERRORE"), err.c_str());
    }
    delete a;
  }
  delete x;
}

void appInfoText(const char* nameIn, HeapBuf& out) {
  char n[APP_CLEAN_BUF] = "";
  cleanAppName(nameIn, n, sizeof n);
  if (!appNameValid(n)) { out.add(tr("Nome app non valido")); out.add('\n'); return; }
  AppInfo* a = new AppInfo(); ErrBuf err;
  if (!loadApp(n, *a, err)) heapAddf(out, "%s: %s\n", tr("ERRORE"), err.c_str());
  else {
    heapAddf(out, "%s (%s) v%s", a->title, a->name, a->ver);
    if (a->author[0]) heapAddf(out, " - %s", a->author);
    out.add('\n');
    heapAddf(out, tr("Permessi: %s"), a->use[0] ? a->use : "-"); out.add('\n');
    heapAddf(out, tr("Chi la usa: %s"), a->who); out.add('\n');
    heapAddf(out, tr("Controlli: %d"), a->nctl); out.add('\n');
    for (int i = 0; i < a->nctl; i++) heapAddf(out, "  %s %s  %s\n", kindName(a->ctl[i].k), a->ctl[i].id, a->ctl[i].label);
    heapAddf(out, tr("Dati iniziali: %d"), a->ndata); out.add('\n');
    heapAddf(out, tr("Codice: %d byte, file: %d byte"), (int)a->codeLen, (int)a->size); out.add('\n');
    out.add(a->icon[0] ? tr("Icona: si") : tr("Icona: no")); out.add('\n');
    out.add(tr("File valido. Si avvia con: app start <nome>")); out.add('\n');
  }
  delete a;
}

static bool roleCan(const AppInfo& a, uint8_t role) { return role >= 2 || !strcmp(a.who, "oper"); }   // l'operatore usa solo le app con who=oper

// {"ok":false,"err":"<msg>","code":"<code>"}
static bool errJson(HeapBuf& out, const char* msg, const char* code) {
  out.add("{\"ok\":false,\"err\":\""); out.addJson(msg, strlen(msg));
  heapAddf(out, "\",\"code\":\"%s\"}", code);
  return !full(out);
}

bool appListJson(HeapBuf& out, uint8_t role) {
  NamesCtx* x = new NamesCtx(); x->n = 0; x->total = 0;
  scanApps(namesCb, x);
  heapAddf(out, "{\"ok\":true,\"max\":%u,\"more\":%s,\"list\":[", (unsigned)APP_FILE_MAX, x->total > x->n ? "true" : "false");
  AppInfo* a = new AppInfo();
  bool first = true;
  for (int i = 0; i < x->n; i++) {
    ErrBuf err;
    bool okl = loadApp(x->names[i], *a, err);
    if (role < 2 && (!okl || !roleCan(*a, role))) continue;     // l'operatore non vede gli errori ne le app solo per Admin
    if (!first) out.add(',');
    first = false;
    if (okl) {
      heapAddf(out, "{\"n\":\"%s\",\"ok\":true,\"t\":\"", a->name); out.addJson(a->title, strlen(a->title));
      heapAddf(out, "\",\"v\":\"%s\",\"a\":\"", a->ver); out.addJson(a->author, strlen(a->author));
      heapAddf(out, "\",\"w\":\"%s\",\"u\":\"%s\",\"c\":%d,\"s\":%u,\"icon\":\"", a->who, a->use, a->nctl, (unsigned)a->size);
      out.addJson(a->icon, strlen(a->icon)); out.add("\"}");
    } else {
      out.add("{\"n\":\""); out.addJson(x->names[i], strlen(x->names[i]));
      out.add("\",\"ok\":false,\"e\":\""); out.addJson(err.c_str(), err.length()); out.add("\"}");
    }
  }
  delete a; delete x;
  out.add("]}");
  return !full(out);
}

bool appInfoJson(const char* nameIn, HeapBuf& out, uint8_t role) {
  char n[APP_CLEAN_BUF] = "";
  cleanAppName(nameIn, n, sizeof n);
  if (!appNameValid(n)) return errJson(out, tr("Nome app non valido"), "error");
  AppInfo* a = new AppInfo(); ErrBuf err;
  if (!loadApp(n, *a, err)) { delete a; return errJson(out, err.c_str(), "error"); }
  if (!roleCan(*a, role)) { delete a; return errJson(out, tr("Il tuo ruolo non permette questa operazione"), "forbidden"); }
  heapAddf(out, "{\"ok\":true,\"n\":\"%s\",\"t\":\"", a->name); out.addJson(a->title, strlen(a->title));
  heapAddf(out, "\",\"v\":\"%s\",\"a\":\"", a->ver); out.addJson(a->author, strlen(a->author));
  heapAddf(out, "\",\"w\":\"%s\",\"u\":\"%s\",\"s\":%u,\"code\":%u,\"data\":%d,\"icon\":\"", a->who, a->use, (unsigned)a->size, (unsigned)a->codeLen, a->ndata);
  out.addJson(a->icon, strlen(a->icon)); out.add("\",\"ctl\":[");
  for (int i = 0; i < a->nctl; i++) {
    if (i) out.add(',');
    heapAddf(out, "{\"k\":\"%s\",\"id\":\"%s\",\"l\":\"", kindName(a->ctl[i].k), a->ctl[i].id);
    out.addJson(a->ctl[i].label, strlen(a->ctl[i].label)); out.add('"');
    if (a->ctl[i].k == 'l') heapAddf(out, ",\"mn\":%ld,\"mx\":%ld", (long)a->ctl[i].mn, (long)a->ctl[i].mx);
    if (a->ctl[i].k == 'g') heapAddf(out, ",\"p\":%d", (int)a->ctl[i].pts);
    out.add('}');
  }
  out.add("]}");
  delete a;
  return !full(out);
}

// ---------- esecuzione ----------
bool appStart(const char* nameIn, ErrBuf& err, uint8_t role, const char* who) {
  err = "";
  char n[APP_CLEAN_BUF] = "";
  cleanAppName(nameIn, n, sizeof n);
  AppInfo* a = new AppInfo();
  if (!loadApp(n, *a, err)) { delete a; return false; }
  if (!roleCan(*a, role)) { err = tr("Il tuo ruolo non permette questa operazione"); delete a; return false; }
  char cur[32];
  if (berryAppRunning(cur, sizeof cur)) { err = !strcmp(n, cur) ? tr("App gia in esecuzione") : tr("Berry e occupato con un altro script"); delete a; return false; }
  if (berryRunning()) { err = tr("Berry e occupato con un altro script"); delete a; return false; }
  rtInit(*a);
  if (!berryStartApp(a->name, a->code, a->use, err)) { rtFree(); delete a; return false; }
  vlog("APP: %s avviata da %s", a->name, who ? who : "?");
  delete a;
  return true;
}

bool appStop(const char* nameIn, ErrBuf& err, uint8_t role) {
  err = "";
  char n[APP_CLEAN_BUF] = "", cur[32];
  cleanAppName(nameIn, n, sizeof n);
  if (!appNameValid(n)) { err = tr("Nome app non valido"); return false; }
  if (!berryAppRunning(cur, sizeof cur) || strcmp(n, cur)) { err = tr("L'app non e in esecuzione"); return false; }
  AppInfo* a = new AppInfo(); ErrBuf e2;
  bool okl = loadApp(n, *a, e2); bool can = !okl || roleCan(*a, role);
  delete a;
  if (!can) { err = tr("Il tuo ruolo non permette questa operazione"); return false; }
  if (!berryStop(err)) return false;
  vlog("APP: %s fermata", n);
  return true;
}

bool appSet(const char* nameIn, const char* id, const char* value, ErrBuf& err, uint8_t role) {
  err = "";
  char n[APP_CLEAN_BUF] = "", cur[32];
  cleanAppName(nameIn, n, sizeof n);
  if (!appNameValid(n)) { err = tr("Nome app non valido"); return false; }
  AppInfo* a = new AppInfo(); ErrBuf e2;
  bool okl = loadApp(n, *a, e2); bool can = okl && roleCan(*a, role);
  delete a;
  if (!okl) { err = e2.c_str(); return false; }
  if (!can) { err = tr("Il tuo ruolo non permette questa operazione"); return false; }
  if (!berryAppRunning(cur, sizeof cur) || strcmp(n, cur)) { err = tr("L'app non e in esecuzione"); return false; }
  return rtSetWeb(id, value, err);
}

bool appStateJson(const char* nameIn, HeapBuf& out, uint8_t role) {
  char n[APP_CLEAN_BUF] = "";
  cleanAppName(nameIn, n, sizeof n);
  if (!appNameValid(n)) return errJson(out, tr("Nome app non valido"), "error");
  AppInfo* a = new AppInfo(); ErrBuf err;
  bool okl = loadApp(n, *a, err); bool can = okl && roleCan(*a, role);
  delete a;
  if (!okl) return errJson(out, err.c_str(), "error");
  if (!can) return errJson(out, tr("Il tuo ruolo non permette questa operazione"), "forbidden");
  char cur[32]; bool run = berryAppRunning(cur, sizeof cur) && !strcmp(n, cur);
  bool mine = berryLastIsApp(n);
  heapAddf(out, "{\"ok\":true,\"n\":\"%s\",\"run\":%s,\"busy\":%s,", n, run ? "true" : "false", berryRunning() ? "true" : "false");
  HeapBuf ob(BERRY_STATE_MAX);                                         // uscita dell'app: heap, non stack
  out.add((mine && berryAppOutJson(ob)) ? ob.c_str() : "\"out\":\"\",\"err\":\"\",\"ms\":0,\"last\":-1");
  out.add(",\"vals\":");
  if (rtLoaded() && rtNameIs(n)) rtValuesJson(out); else out.add("[]");
  out.add('}');
  return !full(out);
}

void appRunText(HeapBuf& out) { berryOutText(out); }
#endif
