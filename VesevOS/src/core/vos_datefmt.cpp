// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_datefmt.cpp
// Formati di data e ora (parte pura, vedi vos_datefmt.h). Gemella di web/src/js_dataora.js.
#include "vos_datefmt.h"
#include <stdio.h>
#include <string.h>

static const char* PRONTA_DATA_CORTA[3] = {"%-d/%-m/%Y", "%Y-%-m-%-d", "%-m/%-d/%Y"};
static const char* PRONTA_DATA_LUNGA = "%A %-d %B %Y";

DfErr dfControlla(const char* f) {
  if (!f || !f[0]) return DF_VUOTO;
  if (strlen(f) > DF_TESTO_MAX) return DF_LUNGO;
  for (const char* p = f; *p; p++) {
    unsigned char c = (unsigned char)*p;
    if (c < 32 || c > 126 || c == '"' || c == '\\') return DF_CARATTERE;
    if (c != '%') continue;
    p++;
    bool senzaZero = false;
    if (*p == '-') {
      senzaZero = true;
      p++;
    }
    if (!*p) return DF_PERCENTO_FINALE;
    const char* ammessi = senzaZero ? "dmHI" : "dmHIMSYybBaAp%";
    if (!strchr(ammessi, *p)) return DF_SEGNO;
  }
  return DF_OK;
}

// Scrive nel buffer senza mai uscire, sempre terminato da zero.
class Uscita {
 public:
  Uscita(char* out, size_t n) : o_(out), n_(n), i_(0) { o_[0] = 0; }
  void car(char c) {
    if (i_ + 1 >= n_) return;
    o_[i_++] = c;
    o_[i_] = 0;
  }
  void testo(const char* s) {
    for (; s && *s; s++) car(*s);
  }
  void numero(int v, bool duecifre) {
    char t[16];
    snprintf(t, sizeof t, duecifre ? "%02d" : "%d", v);
    testo(t);
  }
 private:
  char* o_;
  size_t n_;
  size_t i_;
};

static void togliSpaziEPunti(const char* da, const char* a, char* out, size_t n) {
  while (da < a && (*da == ' ' || *da == '.' || *da == ',')) da++;
  while (a > da && (a[-1] == ' ' || a[-1] == ',')) a--;
  size_t k = (size_t)(a - da);
  if (k >= n) k = n - 1;
  memcpy(out, da, k);
  out[k] = 0;
}

void dfMeseBreve(const char* frase, char* out, size_t n) {
  if (!out || !n) return;
  out[0] = 0;
  if (!frase) return;
  const char* primo = strstr(frase, "%d");
  const char* secondo = primo ? strstr(primo + 2, "%d") : nullptr;
  if (primo && secondo) {
    togliSpaziEPunti(primo + 2, secondo, out, n);
    if (out[0]) return;
  }
  // mese prima del giorno ("Nov %d, %d"): si toglie ogni %d e si puliscono i bordi
  Uscita u(out, n);
  for (const char* p = frase; *p; p++) {
    if (p[0] == '%' && p[1] == 'd') p++;
    else u.car(*p);
  }
  char t[DF_FRASE_MAX];
  togliSpaziEPunti(out, out + strlen(out), t, sizeof t);
  snprintf(out, n, "%s", t);
}

// "giorno mese anno" con il mese abbreviato: il primo %d e il giorno, il secondo l'anno
static void fraseMese(Uscita& u, const char* frase, int giorno, int anno) {
  int k = 0;
  for (const char* p = frase; *p; p++) {
    if (p[0] == '%' && p[1] == 'd') {
      u.numero(k == 0 ? giorno : anno, false);
      k++;
      p++;
    } else {
      u.car(*p);
    }
  }
}

static int ore12(int ore) { return ore % 12 ? ore % 12 : 12; }

static void segno(Uscita& u, char c, bool senzaZero, const DfData& d, const DfNomi* nomi) {
  switch (c) {
    case 'd': u.numero(d.giorno, !senzaZero); break;
    case 'm': u.numero(d.mese, !senzaZero); break;
    case 'H': u.numero(d.ore, !senzaZero); break;
    case 'I': u.numero(ore12(d.ore), !senzaZero); break;
    case 'M': u.numero(d.minuti, true); break;
    case 'S': u.numero(d.secondi, true); break;
    case 'Y': u.numero(d.anno, false); break;
    case 'y': u.numero(d.anno % 100, true); break;
    case 'p': u.testo(d.ore >= 12 ? "PM" : "AM"); break;
    case '%': u.car('%'); break;
    case 'a': if (nomi) u.testo(nomi->giornoBreve[d.giornoSett % 7]); break;
    case 'A': if (nomi) u.testo(nomi->giornoLungo[d.giornoSett % 7]); break;
    case 'B': if (nomi) u.testo(nomi->meseLungo[(d.mese + 11) % 12]); break;
    case 'b':
      if (nomi) {
        char m[DF_NOME_MAX + DF_FRASE_MAX];
        dfMeseBreve(nomi->fraseMese[(d.mese + 11) % 12], m, sizeof m);
        u.testo(m);
      }
      break;
    default: break;
  }
}

void dfFormatta(const char* f, const DfData& d, const DfNomi* nomi, char* out, size_t n) {
  if (!out || !n) return;
  Uscita u(out, n);
  for (const char* p = f ? f : ""; *p; p++) {
    if (*p != '%') {
      u.car(*p);
      continue;
    }
    p++;
    bool senzaZero = false;
    if (*p == '-') {
      senzaZero = true;
      p++;
    }
    if (!*p) break;
    segno(u, *p, senzaZero, d, nomi);
  }
}

void dfDataCorta(const DfImpost& im, const DfData& d, const DfNomi& nomi, char* out, size_t n) {
  bool scritta = im.dataCorta == 3 && dfControlla(im.dataCortaTesto) == DF_OK;
  const char* f = scritta ? im.dataCortaTesto : PRONTA_DATA_CORTA[im.dataCorta >= 0 && im.dataCorta <= 2 ? im.dataCorta : 0];
  dfFormatta(f, d, &nomi, out, n);
}

// Abbreviata: giorno abbreviato e "giorno mese anno" dalla frase del mese (la lingua sceglie ordine e punteggiatura)
static void dataAbbreviata(const DfData& d, const DfNomi& nomi, char* out, size_t n) {
  Uscita u(out, n);
  u.testo(nomi.giornoBreve[d.giornoSett % 7]);
  u.car(' ');
  fraseMese(u, nomi.fraseMese[(d.mese + 11) % 12], d.giorno, d.anno);
}

void dfDataLunga(const DfImpost& im, const DfData& d, const DfNomi& nomi, char* out, size_t n) {
  if (im.dataLunga == 2 && dfControlla(im.dataLungaTesto) == DF_OK) {
    dfFormatta(im.dataLungaTesto, d, &nomi, out, n);
  } else if (im.dataLunga == 1) {
    dfFormatta(PRONTA_DATA_LUNGA, d, &nomi, out, n);
  } else {
    dataAbbreviata(d, nomi, out, n);
  }
}

void dfOraCorta(const DfImpost& im, const DfData& d, char* out, size_t n) {
  dfFormatta(im.ampm ? "%-I:%M %p" : "%-H:%M", d, nullptr, out, n);
}

void dfOraLunga(const DfImpost& im, const DfData& d, const DfNomi& nomi, char* out, size_t n) {
  if (im.oraLunga == 1 && dfControlla(im.oraLungaTesto) == DF_OK) {
    dfFormatta(im.oraLungaTesto, d, &nomi, out, n);
    return;
  }
  const char* f = im.ampm ? (im.oraLungaSec ? "%-I:%M:%S %p" : "%-I:%M %p")
                          : (im.oraLungaSec ? "%-H:%M:%S" : "%-H:%M");
  dfFormatta(f, d, nullptr, out, n);
}
