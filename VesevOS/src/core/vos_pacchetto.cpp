// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_pacchetto.cpp
// Lettura di quello che un pacchetto dichiara di se stesso (regole in vos_pacchetto.h).
#include "vos_pacchetto.h"
#include <string.h>
#include "vos_parse.h"
#include "vos_vdesc.h"

namespace {

struct Coppia { const char* italiano; const char* interno; };

const Coppia kChiavi[] = {
  {"nome", "name"}, {"titolo", "title"}, {"autore", "author"}, {"versione", "version"}, {"licenza", "license"},
  {"avviso", "warn"}, {"chi", "who"}, {"permessi", "use"}, {"classe", "class"}, {"alimentazione", "power"},
  {"livello", "level"}, {"indirizzo", "addr"}, {"frequenza", "freq"}, {"dati", "data"}, {"avvio", "autostart"},
  {"attivo", "active"}, {"testo", "alt"},
};

const size_t kRigheTesta = 24;     // uno script dichiara i suoi dati nelle prime righe di commento

bool sezionePacchetto(std::string_view t) { return viewEqI(t, "[manifest]") || viewEqI(t, "[module]"); }
bool eSezione(std::string_view t) { return t.size() > 2 && t[0] == '[' && t.back() == ']'; }

// Una riga chiave=valore: il primo nome vince (il controllo vero del file vede subito se il secondo e diverso).
void prendiChiave(std::string_view k, std::string_view v, PacDati& d) {
  if (viewEqI(k, "name")) { if (!d.nome[0]) viewCopy(d.nome, sizeof d.nome, v); }
  else if (viewEqI(k, "title")) viewCopy(d.descrizione, sizeof d.descrizione, v.substr(0, sizeof d.descrizione - 1));
  else if (viewEqI(k, "author")) viewCopy(d.autore, sizeof d.autore, v.substr(0, sizeof d.autore - 1));
  else if (viewEqI(k, "who")) viewCopy(d.chi, sizeof d.chi, v);
  else if (viewEqI(k, "version") && pacVersioneOk(v)) viewCopy(d.versione, sizeof d.versione, v);
}

// Commento di uno script: "# versione=1.2" e "# autore=..." sono dati; il primo commento semplice e la descrizione.
void prendiCommento(std::string_view t, PacDati& d) {
  std::string_view c = trimView(t.substr(1));
  size_t q = c.find('=');
  if (q != std::string_view::npos) {
    std::string_view k = pacChiave(trimView(c.substr(0, q)));
    if (viewEqI(k, "version") || viewEqI(k, "author")) { prendiChiave(k, trimView(c.substr(q + 1)), d); return; }
  }
  if (!d.descrizione[0] && !c.empty()) viewCopy(d.descrizione, sizeof d.descrizione, c.substr(0, sizeof d.descrizione - 1));
}

bool soloVersione(std::string_view s) { return s.find_first_of('.') != std::string_view::npos && pacVersioneOk(s); }

// Il prossimo numero di una versione. Finiti i numeri vale 0: 1.2 e 1.2.0 sono la stessa versione.
unsigned long long prossimoNumero(const char*& s) {
  unsigned long long n = 0;
  while (*s >= '0' && *s <= '9') n = n * 10 + (unsigned)(*s++ - '0');
  if (*s) s++;                     // il punto, o qualunque altro carattere: si va sempre avanti
  return n;
}

bool eCpu(std::string_view s) { return viewEqI(s, "s3") || viewEqI(s, "c3") || viewEqI(s, "h2") || viewEqI(s, "c6"); }

}  // namespace

std::string_view pacChiave(std::string_view chiave) {
  for (const Coppia& c : kChiavi) if (viewEqI(chiave, c.italiano)) return c.interno;
  return chiave;
}

bool pacVersioneOk(std::string_view v) {
  if (v.empty() || v.size() > 10) return false;
  for (char c : v) if (!((c >= '0' && c <= '9') || c == '.')) return false;
  return true;
}

// Divide "1.8.17a" in "1.8.17" e 'a'. false = non e una versione del firmware.
static bool divideFirmware(const char* v, char* cifre, size_t cap, char& lettera) {
  size_t n = strlen(v);
  lettera = 0;
  if (n && v[n - 1] >= 'a' && v[n - 1] <= 'z') lettera = v[--n];
  if (n >= cap || !pacVersioneOk(std::string_view(v, n)) || v[0] == '.' || v[n - 1] == '.') return false;
  memcpy(cifre, v, n);
  cifre[n] = 0;
  return true;
}

int pacConfrontaFirmware(const char* a, const char* b) {
  char ca[11], cb[11], la, lb;
  if (!divideFirmware(a, ca, sizeof ca, la) || !divideFirmware(b, cb, sizeof cb, lb)) return PAC_VERSIONE_ILLEGGIBILE;
  if (pacVersionePiuBassa(ca, cb)) return -1;
  if (pacVersionePiuBassa(cb, ca)) return 1;
  return la == lb ? 0 : (la < lb ? -1 : 1);
}

bool pacVersionePiuBassa(const char* nuova, const char* installata) {
  if (!nuova[0] || !installata[0]) return false;
  while (*nuova || *installata) {
    unsigned long long a = prossimoNumero(nuova), b = prossimoNumero(installata);
    if (a != b) return a < b;
  }
  return false;
}

void pacLeggi(std::string_view testo, PacDati& d) {
  memset(&d, 0, sizeof d);
  LineIter it(testo); std::string_view raw;
  bool dentro = false;
  for (size_t riga = 0; it.next(raw); riga++) {
    std::string_view t = trimView(raw);
    if (dentro) {
      if (eSezione(t)) return;
      size_t q = t.find('=');
      if (!t.empty() && t[0] != '#' && q != std::string_view::npos) prendiChiave(pacChiave(trimView(t.substr(0, q))), trimView(t.substr(q + 1)), d);
      continue;
    }
    if (sezionePacchetto(t)) { memset(&d, 0, sizeof d); dentro = true; continue; }    // i commenti prima del manifest non contano
    if (riga >= kRigheTesta || (!t.empty() && t[0] != '#')) return;
    if (!t.empty()) prendiCommento(t, d);
  }
}

bool pacNomeDaFile(std::string_view radice, char* out, size_t cap) {
  size_t trattino = radice.rfind('-');
  if (trattino == std::string_view::npos || trattino == 0 || !soloVersione(radice.substr(trattino + 1))) return false;
  std::string_view resto = radice.substr(0, trattino);
  trattino = resto.rfind('-');
  if (trattino != std::string_view::npos && trattino > 0 && eCpu(resto.substr(trattino + 1))) resto = resto.substr(0, trattino);
  return viewCopyLower(out, cap, resto);
}
