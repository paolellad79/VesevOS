// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_rules_testo.cpp
// Utilita sul testo delle Automazioni (ore, giorni, parole, campi, azioni). Spostate da vos_rules.cpp (file sotto le 500 righe): stesso codice.
#include "vos_rules_int.h"
#include <ctype.h>
#include <string.h>
#include <string_view>
#include "../core/vos_parse.h"

using std::string_view;
namespace vosrules {

bool hhmm(string_view s, uint16_t& out) {
  if (s.size() != 5 || s[2] != ':') return false;
  const int pos[4] = {0, 1, 3, 4};
  for (int p : pos) if (!isdigit((unsigned char)s[p])) return false;
  int h = (s[0] - '0') * 10 + (s[1] - '0'), m = (s[3] - '0') * 10 + (s[4] - '0');
  if (h > 23 || m > 59) return false;
  out = h * 60 + m;
  return true;
}

bool dmaskParse(string_view s, uint8_t& out) {
  if (s.size() != 7) return false;
  uint8_t m = 0;
  for (int i = 0; i < 7; i++) {
    if (s[i] == '1') m |= (1 << i);
    else if (s[i] != '0') return false;
  }
  out = m;
  return true;
}

// parola numero idx (separate da spazi); vista vuota se non c'e
string_view word(string_view s, int idx) {
  size_t i = 0;
  int n = 0;
  while (i < s.size()) {
    while (i < s.size() && s[i] == ' ') i++;
    if (i >= s.size()) break;
    size_t j = i;
    while (j < s.size() && s[j] != ' ') j++;
    if (n == idx) return s.substr(i, j - i);
    n++; i = j;
  }
  return string_view();
}

// tutto il testo dalla parola numero idx in poi
string_view restOf(string_view s, int idx) {
  size_t i = 0;
  int n = 0;
  while (i < s.size()) {
    while (i < s.size() && s[i] == ' ') i++;
    if (i >= s.size()) break;
    if (n == idx) return s.substr(i);
    while (i < s.size() && s[i] != ' ') i++;
    n++;
  }
  return string_view();
}

// campo numero idx di una riga (campi separati da '|')
string_view field(string_view line, int idx) {
  size_t p = 0;
  for (int k = 0; k < idx; k++) {
    p = line.find('|', p);
    if (p == string_view::npos) return string_view();
    p++;
  }
  size_t e = line.find('|', p);
  return line.substr(p, e == string_view::npos ? string_view::npos : e - p);
}

int fieldCount(string_view line) {
  int c = 1;
  for (char ch : line) if (ch == '|') c++;
  return c;
}

// n-esima azione (separate da ';'), senza spazi ai lati
string_view actAt(string_view acts, int n) {
  size_t p = 0;
  for (int k = 0; k < n; k++) {
    p = acts.find(';', p);
    if (p == string_view::npos) return string_view();
    p++;
  }
  size_t e = acts.find(';', p);
  return trimView(acts.substr(p, e == string_view::npos ? string_view::npos : e - p));
}

int actCount(string_view acts) {
  if (acts.empty()) return 0;
  int c = 1;
  for (char ch : acts) if (ch == ';') c++;
  return c;
}

bool hex6ok(string_view s) {
  if (s.size() != 6) return false;
  for (char ch : s) if (!isxdigit((unsigned char)ch)) return false;
  return true;
}

// Copia una parola in un buffer con lo zero finale (per le funzioni che vogliono un const char*). Se non ci sta si taglia:
// i nomi validi sono piu corti, quindi un nome tagliato resta non valido.
void wordTo(char* out, size_t cap, string_view w) {
  size_t n = w.size() < cap - 1 ? w.size() : cap - 1;
  memcpy(out, w.data(), n);
  out[n] = 0;
}

}  // namespace vosrules
