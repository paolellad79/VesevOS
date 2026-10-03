// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_files.cpp
#include "vos_files.h"
#include "vos_i18n.h"
#include "vos_util.h"
#include <LittleFS.h>

#define MAX_TEXT 32768        // editor della pagina: oltre si scarica soltanto

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

// dimensione di una cartella (somma dei file dentro, fino a 8 livelli)
static size_t dirSize(const String& p, int depth) {
  if (depth > 8) return 0;
  File d = LittleFS.open(p);
  if (!d || !d.isDirectory()) return 0;
  size_t t = 0;
  File c = d.openNextFile();
  while (c) {
    if (c.isDirectory()) {
      String nm = String(c.name()); int sl = nm.lastIndexOf('/'); if (sl >= 0) nm = nm.substring(sl + 1);
      t += dirSize((p == "/") ? "/" + nm : p + "/" + nm, depth + 1);
    } else t += c.size();
    c = d.openNextFile();
  }
  return t;
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
           ",\"s\":" + String((unsigned long)(f.isDirectory() ? dirSize(full, 1) : f.size())) + "}";
    }
    f = d.openNextFile();
  }
  j += "],\"used\":" + String((unsigned long)LittleFS.usedBytes()) + ",\"total\":" + String((unsigned long)LittleFS.totalBytes()) +
       ",\"max\":" + String((unsigned long)MAX_TEXT) + "}";
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
  if ((size_t)in.size() + 8192 > LittleFS.totalBytes() - LittleFS.usedBytes()) { in.close(); err = tr("Spazio esaurito"); return false; }
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

// Scrittura sicura: prima un file temporaneo, poi lo scambio. Se manca la corrente
// a meta, il file vecchio resta intero.
bool fsWriteText(const String& raw, const String& text, String& err, bool mustBeNew) {
  String p; if (!okPath(raw, p, err)) return false;
  if (p == "/") { err = tr("Nome file mancante"); return false; }
  if (p.endsWith(".tmp~")) { err = tr("Nome non valido"); return false; }
  if (text.length() > MAX_TEXT) { err = tr("Testo troppo lungo (max 32 KB)"); return false; }
  bool exists = LittleFS.exists(p);
  if (exists) { File d = LittleFS.open(p); bool dir = d && d.isDirectory(); d.close(); if (dir) { err = tr("Esiste una cartella con questo nome"); return false; } }
  if (mustBeNew && exists) { err = tr("Esiste gia"); return false; }
  size_t freeB = LittleFS.totalBytes() - LittleFS.usedBytes();
  if (text.length() + 8192 > freeB) { err = tr("Spazio esaurito"); return false; }
  int ls = p.lastIndexOf('/');
  if (ls > 0 && !LittleFS.exists(p.substring(0, ls))) { err = tr("Cartella non trovata"); return false; }
  String tmp = p + ".tmp~";
  File f = LittleFS.open(tmp, "w");
  if (!f) { err = tr("Impossibile scrivere"); return false; }
  size_t w = f.print(text);
  f.close();
  if (w != text.length()) { LittleFS.remove(tmp); err = tr("Spazio esaurito"); return false; }
  if (exists && !LittleFS.remove(p)) { LittleFS.remove(tmp); err = tr("Impossibile scrivere"); return false; }
  if (!LittleFS.rename(tmp, p)) { err = tr("Impossibile scrivere"); return false; }
  return true;
}

// Legge un file di testo (UTF-8 com'e, accenti compresi). Rifiuta file binari e troppo grandi.
bool fsReadText(const String& raw, String& out, String& err) {
  String p; if (!okPath(raw, p, err)) return false;
  File f = LittleFS.open(p, "r");
  if (!f || f.isDirectory()) { err = tr("File non trovato"); return false; }
  if (f.size() > MAX_TEXT) { f.close(); err = tr("File troppo grande per l'editor (max 32 KB): scaricalo"); return false; }
  out = "";
  if (!out.reserve(f.size() + 1)) { f.close(); err = tr("Memoria insufficiente"); return false; }
  uint8_t buf[256];
  while (f.available()) {
    size_t n = f.read(buf, sizeof(buf));
    for (size_t i = 0; i < n; i++) {
      if (buf[i] == 0) { f.close(); out = ""; err = tr("File binario: non si puo modificare, scaricalo"); return false; }
      out += (char)buf[i];
    }
  }
  f.close();
  return true;
}

// Elenco di tutte le cartelle (per "Sposta"): ["/","/lang",...]
static void dirsRec(const String& p, String& j, int depth, int& n) {
  if (depth > 8 || n >= 64) return;
  File d = LittleFS.open(p);
  if (!d || !d.isDirectory()) return;
  File c = d.openNextFile();
  while (c && n < 64) {
    if (c.isDirectory()) {
      String nm = String(c.name()); int sl = nm.lastIndexOf('/'); if (sl >= 0) nm = nm.substring(sl + 1);
      String full = (p == "/") ? "/" + nm : p + "/" + nm;
      j += ",\"" + jsonEscape(full) + "\""; n++;
      dirsRec(full, j, depth + 1, n);
    }
    c = d.openNextFile();
  }
}

String fsDirsJson() {
  String j = "[\"/\""; int n = 0;
  dirsRec("/", j, 0, n);
  return j + "]";
}

size_t fsTextMax() { return MAX_TEXT; }
