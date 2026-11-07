// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_mod_line.cpp (vedi vos_mod_line.h)
#include "vos_mod_line.h"
#include <string.h>
#include "../core/vos_pacchetto.h"
#include "../core/vos_i18n.h"
#include "../core/vos_util.h"
#include "../core/vos_vdesc.h"

static bool wordOk(std::string_view w, bool firstLetter) {
  if (w.size() < 1 || w.size() > 12) return false;
  for (size_t i = 0; i < w.size(); i++) {
    char c = w[i];
    bool letter = c >= 'a' && c <= 'z';
    if (!(letter || (c >= '0' && c <= '9') || c == '-' || c == '_')) return false;
    if (i == 0 && firstLetter && !letter) return false;
  }
  return true;
}

// Taglia la prossima parola di line (separata da spazi); false se non ce n'e
static bool nextWord(std::string_view& line, std::string_view& w) {
  line = trimView(line);
  if (line.empty()) return false;
  size_t sp = line.find_first_of(" \t");
  w = line.substr(0, sp);
  line = sp == std::string_view::npos ? std::string_view() : line.substr(sp);
  return true;
}

bool modCmdLine(std::string_view line, ModCmd& out, const char** why) {
  memset(&out, 0, sizeof out);
  std::string_view cmd, sub, who, extra;
  const char* msg = tr("Comando: <comando> <sotto> <admin|oper> [testo]");
  if (!nextWord(line, cmd) || !nextWord(line, sub) || !nextWord(line, who)) { *why = msg; return false; }
  bool hasExtra = nextWord(line, extra);
  if (!wordOk(cmd, true) || !wordOk(sub, false)) { *why = tr("Comando o sottocomando non valido (1-12 caratteri: a-z, 0-9, -, _)"); return false; }
  if (who != "admin" && who != "oper") { *why = tr("Chi lo lancia: admin oppure oper"); return false; }
  if (hasExtra && (extra != "testo" || !trimView(line).empty())) { *why = msg; return false; }
  memcpy(out.cmd, cmd.data(), cmd.size()); memcpy(out.sub, sub.data(), sub.size());
  out.admin = who == "admin"; out.rest = hasExtra;
  return true;
}

// ---- widget ----
static bool plainOk(std::string_view s, size_t mx) {                  // ASCII stampabile, niente < > " \ (il titolo finisce in una pagina web)
  if (s.size() < 1 || s.size() > mx) return false;
  for (char ch : s) { unsigned char c = (unsigned char)ch; if (c < 32 || c > 126 || c == '<' || c == '>' || c == '"' || c == '\\') return false; }
  return true;
}

bool modWidgetKey(std::string_view key, std::string_view val, ModWidget& w, const char** why) {
  key = pacChiave(key);
  if (key == "title") {
    if (!plainOk(val, 32)) { *why = tr("Titolo del widget: 1-32 caratteri ASCII, senza < > virgolette o barra"); return false; }
    memcpy(w.title, val.data(), val.size()); w.title[val.size()] = 0;
  } else if (key == "alt") {
    if (!plainOk(val, MODW_ALT_MAX)) { *why = tr("Testo alternativo del widget: 1-48 caratteri ASCII, senza < > virgolette o barra"); return false; }
    memcpy(w.alt, val.data(), val.size()); w.alt[val.size()] = 0;
  } else if (key == "who") {
    if (val != "admin" && val != "oper") { *why = tr("Chi lo vede: admin oppure oper"); return false; }
    memcpy(w.who, val.data(), val.size()); w.who[val.size()] = 0;
  } else { *why = tr("Chiave sconosciuta nel widget (ammesse: title, alt, who)"); return false; }
  return true;
}

// Copia un valore tagliandolo e tenendo solo l'ASCII stampabile (il testo arriva da codice di un driver: non e fidato)
static void putText(char* dst, size_t mx, const char* src, size_t n) {
  size_t k = 0;
  for (size_t i = 0; i < n && k < mx; i++) { unsigned char c = (unsigned char)src[i]; if (c >= 32 && c <= 126) dst[k++] = (char)c; }
  dst[k] = 0;
}

void modWidgetParseOut(const char* out, ModWidgetVals& vals) {
  memset(&vals, 0, sizeof vals);
  strcpy(vals.st, "off");                                              // se il codice non dice "ok", il widget e spento
  const char* p = out ? out : "";
  while (*p) {
    const char* e = strchr(p, '\n'); size_t n = e ? (size_t)(e - p) : strlen(p);
    if (n && p[n - 1] == '\r') n--;
    if (n >= 3 && p[0] == 's' && p[1] == 't' && p[2] == '=') { if (n == 5 && p[3] == 'o' && p[4] == 'k') strcpy(vals.st, "ok"); else strcpy(vals.st, "off"); }
    else if (n >= 2 && p[0] == 'v' && p[1] == '=') putText(vals.v, MODW_V_MAX, p + 2, n - 2);
    else if (n >= 2 && p[0] == 's' && p[1] == '=') putText(vals.s, MODW_S_MAX, p + 2, n - 2);
    if (!e) break;
    p = e + 1;
  }
}
