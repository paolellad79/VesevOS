// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// VesevOS - vos_files.cpp
#include "vos_files.h"
#include "vos_i18n.h"
#include "vos_util.h"
#include <LittleFS.h>

#define MAX_TEXT 8000

String fsClean(const String& in) {
  String p = in; p.trim();
  if (p.length() == 0) p = "/";
  if (p[0] != '/') p = "/" + p;
  if (p.length() > 100) return "";
  if (p.indexOf("..") >= 0 || p.indexOf("//") >= 0 || p.indexOf('\\') >= 0) return "";
  for (size_t i = 0; i < p.length(); i++) {
    unsigned char c = (unsigned char)p[i];
    if (c < 32 || c >= 127) return "";
  }
  if (p.length() > 1 && p[p.length() - 1] == '/') p.remove(p.length() - 1);
  return p;
}

bool fsProtected(const String& p) {
  return p.startsWith("/vesevos.conf");
}

static bool okPath(const String& raw, String& p, String& err) {
  p = fsClean(raw);
  if (p.length() == 0) { err = tr("Percorso non valido"); return false; }
  if (fsProtected(p)) { err = tr("File di sistema protetto"); return false; }
  return true;
}

String fsListJson(const String& raw) {
  String p = fsClean(raw);
  if (p.length() == 0) return String("{\"ok\":false,\"err\":\"") + jsonEscape(tr("Percorso non valido")) + "\"}";
  File d = LittleFS.open(p);
  if (!d || !d.isDirectory()) return String("{\"ok\":false,\"err\":\"") + jsonEscape(tr("Cartella non trovata")) + "\"}";
  String j = "{\"ok\":true,\"path\":\"" + jsonEscape(p) + "\",\"list\":[";
  bool first = true;
  File f = d.openNextFile();
  while (f) {
    String name = String(f.name());
    int sl = name.lastIndexOf('/');
    if (sl >= 0) name = name.substring(sl + 1);
    String full = (p == "/") ? "/" + name : p + "/" + name;
    if (!fsProtected(full)) {
      if (!first) j += ",";
      first = false;
      j += "{\"n\":\"" + jsonEscape(name) + "\",\"d\":" + String(f.isDirectory() ? "true" : "false") +
           ",\"s\":" + String((unsigned long)f.size()) + "}";
    }
    f = d.openNextFile();
  }
  j += "],\"used\":" + String((unsigned long)LittleFS.usedBytes()) + ",\"total\":" + String((unsigned long)LittleFS.totalBytes()) + "}";
  return j;
}

bool fsMkdir(const String& raw, String& err) {
  String p; if (!okPath(raw, p, err)) return false;
  if (LittleFS.exists(p)) { err = tr("Esiste gia"); return false; }
  if (!LittleFS.mkdir(p)) { err = tr("Impossibile creare la cartella"); return false; }
  return true;
}

static bool rmRec(const String& p, int depth) {
  File f = LittleFS.open(p);
  if (!f) return false;
  if (!f.isDirectory()) { f.close(); return LittleFS.remove(p); }
  if (depth > 8) { f.close(); return false; }
  // raccolgo i nomi prima di cancellare
  String names[24]; int n = 0;
  File c = f.openNextFile();
  while (c && n < 24) {
    String nm = String(c.name()); int sl = nm.lastIndexOf('/'); if (sl >= 0) nm = nm.substring(sl + 1);
    names[n++] = nm;
    c = f.openNextFile();
  }
  f.close();
  for (int i = 0; i < n; i++) if (!rmRec((p == "/" ? "" : p) + "/" + names[i], depth + 1)) return false;
  if (n == 24) return rmRec(p, depth);       // ce ne sono altri: ripeto
  return LittleFS.rmdir(p);
}

bool fsRemove(const String& raw, String& err) {
  String p; if (!okPath(raw, p, err)) return false;
  if (p == "/") { err = tr("Non si puo eliminare la radice"); return false; }
  if (!LittleFS.exists(p)) { err = tr("Non trovato"); return false; }
  if (!rmRec(p, 0)) { err = tr("Eliminazione non riuscita"); return false; }
  return true;
}

bool fsRename(const String& a, const String& b, String& err) {
  String pa, pb;
  if (!okPath(a, pa, err) || !okPath(b, pb, err)) return false;
  if (!LittleFS.exists(pa)) { err = tr("Origine non trovata"); return false; }
  if (LittleFS.exists(pb)) { err = tr("La destinazione esiste gia"); return false; }
  if (!LittleFS.rename(pa, pb)) { err = tr("Rinomina non riuscita"); return false; }
  return true;
}

bool fsCopy(const String& a, const String& b, String& err) {
  String pa, pb;
  if (!okPath(a, pa, err) || !okPath(b, pb, err)) return false;
  File in = LittleFS.open(pa, "r");
  if (!in || in.isDirectory()) { err = tr("Origine non e un file"); return false; }
  if (LittleFS.exists(pb)) { in.close(); err = tr("La destinazione esiste gia"); return false; }
  File out = LittleFS.open(pb, "w");
  if (!out) { in.close(); err = tr("Impossibile creare la destinazione"); return false; }
  uint8_t buf[512]; bool ok = true;
  while (in.available()) {
    size_t n = in.read(buf, sizeof(buf));
    if (out.write(buf, n) != n) { ok = false; break; }
  }
  in.close(); out.close();
  if (!ok) { LittleFS.remove(pb); err = tr("Spazio esaurito"); return false; }
  return true;
}

bool fsWriteText(const String& raw, const String& text, String& err) {
  String p; if (!okPath(raw, p, err)) return false;
  if (p == "/") { err = tr("Nome file mancante"); return false; }
  if (text.length() > MAX_TEXT) { err = tr("Testo troppo lungo (max 8000)"); return false; }
  File f = LittleFS.open(p, "w");
  if (!f) { err = tr("Impossibile scrivere"); return false; }
  size_t w = f.print(text);
  f.close();
  if (w != text.length()) { err = tr("Spazio esaurito"); return false; }
  return true;
}

bool fsReadText(const String& raw, String& out, String& err) {
  String p; if (!okPath(raw, p, err)) return false;
  File f = LittleFS.open(p, "r");
  if (!f || f.isDirectory()) { err = tr("File non trovato"); return false; }
  out = "";
  while (f.available() && out.length() < MAX_TEXT) {
    char c = f.read();
    out += (c == '\n' || c == '\t' || (c >= 32 && c < 127)) ? c : '?';
  }
  f.close();
  return true;
}
