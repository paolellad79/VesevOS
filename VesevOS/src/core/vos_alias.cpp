// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_alias.cpp
// Alias e descrizione delle periferiche: tabella, controllo dei valori, lettura e scrittura nella configurazione e JSON per le impostazioni.
#include "vos_alias.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace {

struct Voce {
  char nome[ALIAS_NAME_MAX + 1];
  char alias[ALIAS_LEN_MAX + 1];
  char descr[ALIAS_DESCR_MAX + 1];
};

Voce* g_voci = nullptr;      // creata alla prima voce (circa 7 KB), mai liberata
int   g_n = 0;

int cerca(const char* nome) {
  for (int i = 0; i < g_n; i++) {
    if (!strcmp(g_voci[i].nome, nome)) return i;
  }
  return -1;
}

void togliIndice(int i) {
  for (int k = i; k < g_n - 1; k++) g_voci[k] = g_voci[k + 1];
  g_n--;
}

// Lunghezza (1-4) del carattere UTF-8 che inizia in s, 0 se non e ben formato (sequenze troppo lunghe, surrogati e valori oltre U+10FFFF compresi).
int lunghezzaUtf8(const unsigned char* s, uint32_t& cp) {
  unsigned char c = s[0];
  if (c < 0x80) { cp = c; return 1; }
  int n;
  if (c >= 0xC2 && c <= 0xDF) { n = 2; cp = c & 0x1F; }
  else if (c >= 0xE0 && c <= 0xEF) { n = 3; cp = c & 0x0F; }
  else if (c >= 0xF0 && c <= 0xF4) { n = 4; cp = c & 0x07; }
  else return 0;
  for (int k = 1; k < n; k++) {
    if ((s[k] & 0xC0) != 0x80) return 0;
    cp = (cp << 6) | (s[k] & 0x3F);
  }
  if (n == 3 && cp < 0x800) return 0;
  if (n == 4 && (cp < 0x10000 || cp > 0x10FFFF)) return 0;
  if (cp >= 0xD800 && cp <= 0xDFFF) return 0;
  return n;
}

bool cpVietato(uint32_t cp) {
  if (cp < 0x20 || cp == 0x7F) return true;                 // controlli
  if (cp >= 0x80 && cp <= 0x9F) return true;                // controlli C1
  if (cp == '<' || cp == '>' || cp == '"' || cp == '\\') return true;
  if (cp == 0x2028 || cp == 0x2029) return true;            // a capo Unicode
  if (cp >= 0x202A && cp <= 0x202E) return true;           // inversione della scrittura
  if (cp >= 0x2066 && cp <= 0x2069) return true;
  if (cp == 0x200E || cp == 0x200F) return true;
  if (cp == 0xFEFF) return true;
  return false;
}

char minuscolo(char c) { return (c >= 'A' && c <= 'Z') ? (char)(c + ('a' - 'A')) : c; }

bool ugualiSenzaMaiuscole(const char* a, const char* b) {
  while (*a && *b) {
    if (minuscolo(*a) != minuscolo(*b)) return false;
    a++;
    b++;
  }
  return *a == *b;
}

// Posto per la voce (creata se manca). nullptr = tabella piena o memoria finita.
Voce* trovaOCrea(const char* nome) {
  int i = g_voci ? cerca(nome) : -1;
  if (i >= 0) return &g_voci[i];
  if (!g_voci) {
    g_voci = (Voce*)calloc(ALIAS_N, sizeof(Voce));
    if (!g_voci) return nullptr;
  }
  if (g_n >= ALIAS_N) return nullptr;
  Voce* v = &g_voci[g_n++];
  memset(v, 0, sizeof *v);
  strncpy(v->nome, nome, ALIAS_NAME_MAX);
  return v;
}

void togliSeVuota(const char* nome) {
  int i = g_voci ? cerca(nome) : -1;
  if (i >= 0 && !g_voci[i].alias[0] && !g_voci[i].descr[0]) togliIndice(i);
}

}  // namespace

AliasErr aliasTestoCheck(const char* testo, size_t maxByte) {
  size_t n = strlen(testo);
  if (n > maxByte) return ALIAS_LUNGO;
  if (n && (testo[0] == ' ' || testo[n - 1] == ' ')) return ALIAS_SPAZI;
  const unsigned char* p = (const unsigned char*)testo;
  while (*p) {
    uint32_t cp = 0;
    int len = lunghezzaUtf8(p, cp);
    if (!len || cpVietato(cp)) return ALIAS_CARATTERI;
    p += len;
  }
  return ALIAS_OK;
}

bool aliasNomeOk(const char* nome) {
  size_t n = strlen(nome);
  if (n < 1 || n > ALIAS_NAME_MAX) return false;
  for (size_t i = 0; i < n; i++) {
    char c = nome[i];
    if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_')) return false;
  }
  return true;
}

const char* aliasGet(const char* nome) {
  int i = g_voci ? cerca(nome) : -1;
  return i >= 0 ? g_voci[i].alias : "";
}

const char* aliasDescr(const char* nome) {
  int i = g_voci ? cerca(nome) : -1;
  return i >= 0 ? g_voci[i].descr : "";
}

AliasErr aliasSet(const char* nome, const char* alias, const char* descr) {
  if (!aliasNomeOk(nome)) return ALIAS_NOME;
  AliasErr why = alias ? aliasTestoCheck(alias, ALIAS_LEN_MAX) : ALIAS_OK;
  if (why == ALIAS_OK && descr) why = aliasTestoCheck(descr, ALIAS_DESCR_MAX);
  if (why != ALIAS_OK) return why;
  Voce* v = trovaOCrea(nome);
  if (!v) return ALIAS_PIENO;
  if (alias) strcpy(v->alias, alias);
  if (descr) strcpy(v->descr, descr);
  togliSeVuota(nome);
  return ALIAS_OK;
}

const char* aliasDuplicato(const char* alias, const char* tranne) {
  if (!alias[0]) return nullptr;
  for (int i = 0; i < g_n; i++) {
    if (!strcmp(g_voci[i].nome, tranne)) continue;
    if (ugualiSenzaMaiuscole(g_voci[i].alias, alias)) return g_voci[i].nome;
  }
  return nullptr;
}

bool aliasSposta(const char* da, const char* a) {
  int i = g_voci ? cerca(da) : -1;
  if (i < 0) return false;
  int k = cerca(a);
  if (k >= 0) togliIndice(k);                                // la destinazione non aveva diritto a niente: si sostituisce
  i = cerca(da);
  strncpy(g_voci[i].nome, a, ALIAS_NAME_MAX);
  g_voci[i].nome[ALIAS_NAME_MAX] = 0;
  return true;
}

bool aliasTogli(const char* nome) {
  int i = g_voci ? cerca(nome) : -1;
  if (i < 0) return false;
  togliIndice(i);
  return true;
}

void aliasAzzera() { g_n = 0; }
int  aliasConta() { return g_n; }
const char* aliasNomeAt(int i) { return (i >= 0 && i < g_n) ? g_voci[i].nome : ""; }

void aliasImportaInizio() { g_n = 0; }

bool aliasImportaChiave(std::string_view chiave, std::string_view valore) {
  size_t dot = chiave.rfind('.');
  if (dot == std::string_view::npos || dot == 0) return false;
  char nome[ALIAS_NAME_MAX + 2];
  if (dot > ALIAS_NAME_MAX) return false;
  memcpy(nome, chiave.data(), dot);
  nome[dot] = 0;
  if (!aliasNomeOk(nome)) return false;
  std::string_view campo = chiave.substr(dot + 1);
  char testo[ALIAS_DESCR_MAX + 2];
  if (valore.size() > ALIAS_DESCR_MAX) return false;
  memcpy(testo, valore.data(), valore.size());
  testo[valore.size()] = 0;
  if (campo == "alias") return aliasSet(nome, testo, nullptr) == ALIAS_OK;
  if (campo == "descrizione") return aliasSet(nome, nullptr, testo) == ALIAS_OK;
  return false;
}

void aliasEsporta(void (*scrivi)(void* ctx, const char* chiave, const char* valore), void* ctx) {
  for (int i = 0; i < g_n; i++) {
    char k[ALIAS_NAME_MAX + 16];
    if (g_voci[i].alias[0]) {
      snprintf(k, sizeof k, "%s.alias", g_voci[i].nome);
      scrivi(ctx, k, g_voci[i].alias);
    }
    if (g_voci[i].descr[0]) {
      snprintf(k, sizeof k, "%s.descrizione", g_voci[i].nome);
      scrivi(ctx, k, g_voci[i].descr);
    }
  }
}

// Testo dentro virgolette JSON. I caratteri vietati non entrano mai nella tabella, ma si protegge lo stesso; i byte UTF-8 (>= 0x80) passano.
static void testoJson(HeapBuf& out, const char* s) {
  for (; *s; s++) {
    unsigned char c = (unsigned char)*s;
    if (c == '"' || c == '\\') { out.add('\\'); out.add((char)c); }
    else if (c < 0x20 || c == 0x7F) out.add('?');
    else out.add((char)c);
  }
}

bool aliasJson(HeapBuf& out) {
  out.add('{');
  for (int i = 0; i < g_n; i++) {
    if (i) out.add(',');
    out.add('"');
    out.add(g_voci[i].nome);
    out.add("\":{\"a\":\"");
    testoJson(out, g_voci[i].alias);
    out.add("\",\"d\":\"");
    testoJson(out, g_voci[i].descr);
    out.add("\"}");
  }
  out.add('}');
  return !out.oom() && !out.over();
}
