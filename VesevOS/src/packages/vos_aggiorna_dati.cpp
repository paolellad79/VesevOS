// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_aggiorna_dati.cpp
#include "vos_aggiorna_dati.h"
#include <stdio.h>
#include <string.h>
#include "../core/vos_pacchetto.h"

namespace {

const int kProfonditaJson = 5;       // gli elenchi sono piatti: un testo piu annidato non e il nostro
const int kChiaveMax = 12;           // la chiave piu lunga che ci interessa e "publisher"

// Il testo viene da internet: ogni passo controlla la fine e un errore ferma la lettura.
struct Lettore {
  const char* p;
  const char* fine;
};

// Un campo atteso: al massimo una delle tre destinazioni. Gli altri campi si controllano e si saltano.
struct Campo {
  const char* nome;
  char*       testo;
  size_t      cap;
  uint32_t*   numero;
  Campo*      figli;
  int         nFigli;
  bool        visto;
};

// Dove finisce un testo: inservibile = troppo lungo o con un carattere che non si puo tenere.
struct Uscita {
  char*  a;
  size_t cap;
  size_t n;
  bool   inservibile;
};

bool saltaValore(Lettore& l, int prof);

void metti(Uscita& u, char c) {
  if (!u.a) return;
  if (u.n + 1 >= u.cap) {
    u.inservibile = true;
    return;
  }
  u.a[u.n++] = c;
}

// Il punto di codice in UTF-8. Le coppie surrogate e lo zero non servono nei campi che teniamo.
void mettiCodice(Uscita& u, unsigned c) {
  if (c == 0 || (c >= 0xD800 && c <= 0xDFFF)) {
    u.inservibile = true;
  } else if (c < 0x80) {
    metti(u, (char)c);
  } else if (c < 0x800) {
    metti(u, (char)(0xC0 | (c >> 6)));
    metti(u, (char)(0x80 | (c & 0x3F)));
  } else {
    metti(u, (char)(0xE0 | (c >> 12)));
    metti(u, (char)(0x80 | ((c >> 6) & 0x3F)));
    metti(u, (char)(0x80 | (c & 0x3F)));
  }
}

void saltaSpazi(Lettore& l) {
  while (l.p < l.fine && (*l.p == ' ' || *l.p == '\t' || *l.p == '\n' || *l.p == '\r')) l.p++;
}

bool leggiEsa(Lettore& l, unsigned& codice) {
  if (l.fine - l.p < 4) return false;
  codice = 0;
  for (int i = 0; i < 4; i++) {
    char c = *l.p++;
    unsigned cifra;
    if (c >= '0' && c <= '9') cifra = c - '0';
    else if (c >= 'a' && c <= 'f') cifra = c - 'a' + 10;
    else if (c >= 'A' && c <= 'F') cifra = c - 'A' + 10;
    else return false;
    codice = codice * 16 + cifra;
  }
  return true;
}

// La lettera dopo la barra rovesciata. false = sequenza non valida.
bool leggiEscape(Lettore& l, Uscita& u) {
  if (l.p >= l.fine) return false;
  char e = *l.p++;
  switch (e) {
    case '"': case '\\': case '/': metti(u, e); return true;
    case 'b': metti(u, '\b'); return true;
    case 'f': metti(u, '\f'); return true;
    case 'n': metti(u, '\n'); return true;
    case 'r': metti(u, '\r'); return true;
    case 't': metti(u, '\t'); return true;
    case 'u': break;
    default: return false;
  }
  unsigned codice;
  if (!leggiEsa(l, codice)) return false;
  mettiCodice(u, codice);
  return true;
}

// l.p e sulle virgolette di apertura. a = nullptr: si controlla soltanto.
bool leggiStringa(Lettore& l, char* a, size_t cap, bool& inservibile) {
  Uscita u = {a, cap, 0, false};
  l.p++;
  while (l.p < l.fine) {
    unsigned char c = (unsigned char)*l.p++;
    if (c == '"') {
      if (a) a[u.n] = 0;
      inservibile = u.inservibile;
      return true;
    }
    if (c < 0x20) return false;
    if (c != '\\') metti(u, (char)c);
    else if (!leggiEscape(l, u)) return false;
  }
  return false;
}

bool cifra(const Lettore& l) { return l.p < l.fine && *l.p >= '0' && *l.p <= '9'; }

// Salta una o piu cifre. false se non ce n'e nemmeno una.
bool saltaCifre(Lettore& l) {
  if (!cifra(l)) return false;
  while (cifra(l)) l.p++;
  return true;
}

// Numero JSON. In valore solo un intero senza segno che entra in 32 bit, altrimenti 0: per noi 0 e "non valido".
bool leggiNumero(Lettore& l, uint32_t* valore) {
  bool intero = true;
  uint64_t n = 0;
  if (l.p < l.fine && *l.p == '-') {
    intero = false;
    l.p++;
  }
  if (!cifra(l)) return false;
  if (*l.p == '0') {
    l.p++;
    if (cifra(l)) return false;              // zeri in testa: non e JSON
  } else {
    while (cifra(l)) {
      if (n <= UINT32_MAX) n = n * 10 + (unsigned)(*l.p - '0');
      l.p++;
    }
  }
  if (l.p < l.fine && *l.p == '.') {
    intero = false;
    l.p++;
    if (!saltaCifre(l)) return false;
  }
  if (l.p < l.fine && (*l.p == 'e' || *l.p == 'E')) {
    intero = false;
    l.p++;
    if (l.p < l.fine && (*l.p == '+' || *l.p == '-')) l.p++;
    if (!saltaCifre(l)) return false;
  }
  if (valore) *valore = intero && n <= UINT32_MAX ? (uint32_t)n : 0;
  return true;
}

bool parola(Lettore& l, const char* testo) {
  size_t n = strlen(testo);
  if ((size_t)(l.fine - l.p) < n || memcmp(l.p, testo, n) != 0) return false;
  l.p += n;
  return true;
}

bool saltaElenco(Lettore& l, int prof) {
  if (prof >= kProfonditaJson) return false;
  l.p++;
  saltaSpazi(l);
  if (l.p < l.fine && *l.p == ']') {
    l.p++;
    return true;
  }
  while (l.p < l.fine) {
    if (!saltaValore(l, prof + 1)) return false;
    saltaSpazi(l);
    if (l.p >= l.fine) return false;
    char c = *l.p++;
    if (c == ']') return true;
    if (c != ',') return false;
  }
  return false;
}

// Il valore di un campo atteso. Un tipo diverso si salta: il campo resta vuoto e lo giudicano i controlli di aggLeggiUltima.
bool leggiCampo(Lettore& l, Campo& c, int prof);

// l.p e sulla graffa di apertura. Un campo doppio e sospetto: il testo si rifiuta.
bool leggiOggetto(Lettore& l, Campo* campi, int nCampi, int prof) {
  if (prof >= kProfonditaJson) return false;
  l.p++;
  saltaSpazi(l);
  if (l.p < l.fine && *l.p == '}') {
    l.p++;
    return true;
  }
  while (l.p < l.fine) {
    char chiave[kChiaveMax + 1];
    bool lunga = false;
    saltaSpazi(l);
    if (l.p >= l.fine || *l.p != '"' || !leggiStringa(l, chiave, sizeof chiave, lunga)) return false;
    saltaSpazi(l);
    if (l.p >= l.fine || *l.p != ':') return false;
    l.p++;
    Campo* trovato = nullptr;
    for (int i = 0; !lunga && i < nCampi; i++) {
      if (!strcmp(chiave, campi[i].nome)) trovato = &campi[i];
    }
    if (trovato && trovato->visto) return false;
    if (trovato) trovato->visto = true;
    if (trovato ? !leggiCampo(l, *trovato, prof) : !saltaValore(l, prof)) return false;
    saltaSpazi(l);
    if (l.p >= l.fine) return false;
    char c = *l.p++;
    if (c == '}') return true;
    if (c != ',') return false;
  }
  return false;
}

bool leggiCampo(Lettore& l, Campo& c, int prof) {
  saltaSpazi(l);
  if (l.p >= l.fine) return false;
  char primo = *l.p;
  if (c.figli && primo == '{') return leggiOggetto(l, c.figli, c.nFigli, prof + 1);
  if (c.numero && (primo == '-' || cifra(l))) return leggiNumero(l, c.numero);
  if (c.testo && primo == '"') {
    bool inservibile = false;
    return leggiStringa(l, c.testo, c.cap, inservibile) && !inservibile;   // un valore tagliato sarebbe un valore sbagliato
  }
  return saltaValore(l, prof);
}

bool saltaValore(Lettore& l, int prof) {
  saltaSpazi(l);
  if (l.p >= l.fine) return false;
  char primo = *l.p;
  bool ignorato = false;
  if (primo == '{') return leggiOggetto(l, nullptr, 0, prof + 1);
  if (primo == '[') return saltaElenco(l, prof);
  if (primo == '"') return leggiStringa(l, nullptr, 0, ignorato);
  if (primo == '-' || cifra(l)) return leggiNumero(l, nullptr);
  return parola(l, "true") || parola(l, "false") || parola(l, "null");
}

bool testoVisibile(const char* s) {
  for (; *s; s++) if ((unsigned char)*s <= 32 || (unsigned char)*s > 126) return false;
  return true;
}

bool impronta(const char* s) {
  if (strlen(s) != 64) return false;
  for (; *s; s++) if (!((*s >= '0' && *s <= '9') || (*s >= 'a' && *s <= 'f') || (*s >= 'A' && *s <= 'F'))) return false;
  return true;
}

}  // namespace

bool aggLeggiUltima(const char* testo, size_t n, AggDisponibile& d) {
  memset(&d, 0, sizeof d);
  uint32_t schema = 0;
  Campo firmware[] = {
    {"version", d.ver, sizeof d.ver, nullptr, nullptr, 0, false},
    {"url", d.url, sizeof d.url, nullptr, nullptr, 0, false},
    {"sha256", d.sha, sizeof d.sha, nullptr, nullptr, 0, false},
    {"size", nullptr, 0, &d.dim, nullptr, 0, false},
    {"date", d.data, sizeof d.data, nullptr, nullptr, 0, false},
    {"publisher", d.chi, sizeof d.chi, nullptr, nullptr, 0, false},
    {"channel", d.canale, sizeof d.canale, nullptr, nullptr, 0, false},
  };
  Campo recovery[] = {{"version", d.verRec, sizeof d.verRec, nullptr, nullptr, 0, false}};
  Campo radice[] = {
    {"schema", nullptr, 0, &schema, nullptr, 0, false},
    {"firmware", nullptr, 0, nullptr, firmware, sizeof firmware / sizeof firmware[0], false},
    {"recovery", nullptr, 0, nullptr, recovery, 1, false},
  };
  Lettore l = {testo, testo + n};
  saltaSpazi(l);
  if (l.p >= l.fine || *l.p != '{') return false;
  if (!leggiOggetto(l, radice, sizeof radice / sizeof radice[0], 0)) return false;
  saltaSpazi(l);
  if (l.p != l.fine) return false;                 // altro testo dopo la fine: non e un elenco valido
  if (schema != 1) return false;
  if (!d.dim || !d.ver[0] || !impronta(d.sha) || !testoVisibile(d.url) || strncmp(d.url, "https://", 8) != 0) return false;
  return pacConfrontaFirmware(d.ver, d.ver) == 0;
}

bool aggSpazioRiserva(uint32_t liberiKb, uint32_t byteImmagine) {
  return liberiKb >= byteImmagine / 1024 + 1 + AGG_MARGINE_SD_KB;
}

bool aggTentativoAmmesso(const char* ultima, uint32_t fatti, const char* versione) {
  return strcmp(ultima, versione) != 0 || fatti < AGG_TENTATIVI_MAX;
}

bool aggTentativiDaAzzerare(const char* ultima, const char* installata) {
  return ultima[0] && pacConfrontaFirmware(ultima, installata) != 1;
}

bool aggOrarioGiusto(const struct tm& t, uint8_t freq, uint8_t giorno, uint16_t ora) {
  if (t.tm_hour * 60 + t.tm_min != ora) return false;
  if (freq == 1) return (t.tm_wday + 6) % 7 == (giorno ? giorno - 1 : 0) % 7;
  if (freq == 2) return t.tm_mday == giorno;
  return true;
}

bool aggTestaFirmware(const uint8_t* dati, size_t n) {
  return n >= 14 && dati[0] == 0xE9 && dati[12] == 0x09 && dati[13] == 0x00;
}

bool aggMarchio(const uint8_t* dati, size_t n, size_t& pos) {
  static const char kMarchio[] = "{FW:VesevOS}";
  const size_t lung = sizeof kMarchio - 1;
  for (size_t i = 0; i < n && pos < lung; i++) {
    if (dati[i] == (uint8_t)kMarchio[pos]) pos++;
    else pos = dati[i] == (uint8_t)kMarchio[0] ? 1 : 0;
  }
  return pos == lung;
}
