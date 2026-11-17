// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_i18n_elenco.cpp
// Elenco delle lingue installate (/lang/<codice>.vld): nome, codice locale e bandiera si leggono dalla testata del
// dizionario (le prime righe dopo l'intestazione), senza toccare le frasi.
#include "vos_i18n.h"
#include "vos_i18n_int.h"
#include "../drivers/vos_drv_fs.h"
#include "vos_lang_it.h"
#include <stdio.h>

// Testi letti dalla testata di un dizionario. Un solo blocco di heap per tutto (la bandiera pesa fino a 1 KB).
struct LangHead {
  char raw[2049];
  char name[LANG_NAME_BUF * 2];
  char loc[16];
  char flag[1025];
};

static void fileHead(const char* code, LangHead& h) {
  snprintf(h.name, sizeof h.name, "%s", code);
  h.loc[0] = 0;
  h.flag[0] = 0;
  char path[sizeof(LANG_DIR) + LANG_CODE_BUF + 6];
  langPath(code, "vld", path, sizeof path);
  VosFile f = drvFsOpen(path, "r");
  LangVld vld;
  if (!f || !langLeggiIntest(f, vld)) return;
  size_t len = vld.offTab - LANG_INTEST;                 // la testata finisce dove comincia la tabella
  if (len > sizeof(h.raw) - 1) len = sizeof(h.raw) - 1;
  if (!f.seek(LANG_INTEST)) return;
  size_t n = f.read((uint8_t*)h.raw, len);
  f.close();
  h.raw[n] = 0;
  char nm[sizeof h.name];
  if (langTestataValore(h.raw, n, "nome=", nm, sizeof nm) && nm[0]) snprintf(h.name, sizeof h.name, "%s", nm);
  langTestataValore(h.raw, n, "locale=", h.loc, sizeof h.loc);
  bool whole = langTestataValore(h.raw, n, "bandiera=", h.flag, sizeof h.flag);
  if (!whole || strncmp(h.flag, "<svg", 4) != 0) h.flag[0] = 0;   // la pagina la mostra come immagine (<img>): niente script
}

static bool itemJson(HeapBuf& out, const char* code, const char* name, const char* loc, const char* flag) {
  return out.add("{\"code\":\"") && out.add(code) && out.add("\",\"name\":\"") && out.addJson(name) &&
         out.add("\",\"loc\":\"") && out.addJson(loc) && out.add("\",\"flag\":\"") && out.addJson(flag) && out.add("\"}");
}

// Prossimo file /lang/<codice>.vld con codice valido e diverso da "it": scrive il codice in code. false = finiti.
static bool nextLangCode(VosFile& d, char* code) {
  for (VosFile e = d.openNextFile(); e; e = d.openNextFile()) {
    if (e.isDirectory()) continue;
    const char* n = e.name();
    const char* sl = strrchr(n, '/'); if (sl) n = sl + 1;
    size_t len = strlen(n);
    if (len < 6 || strcmp(n + len - 4, ".vld") != 0) continue;
    std::string_view c(n, len - 4);
    if (c == "it" || !langCodeValid(c)) continue;
    snprintf(code, LANG_CODE_BUF, "%.*s", (int)c.size(), c.data());
    return true;
  }
  return false;
}

bool langListJson(HeapBuf& out) {
  bool ok = out.add('[') && itemJson(out, "it", "Italiano", "it-IT", LANG_FLAG_IT);
  VosFile d = drvFsOpen(LANG_DIR);
  if (ok && d && d.isDirectory()) {
    LangHead* h = (LangHead*)malloc(sizeof(LangHead));
    if (!h) { d.close(); return false; }
    char code[LANG_CODE_BUF];
    int n = 1;                                                          // "it" e gia nell'elenco
    while (ok && n < LANG_LIST_MAX && nextLangCode(d, code)) {
      n++;
      fileHead(code, *h);
      ok = out.add(',') && itemJson(out, code, h->name, h->loc, h->flag);
    }
    free(h);
  }
  if (d) d.close();
  return ok && out.add(']');
}

int langList(char codes[][LANG_CODE_BUF], char names[][LANG_NAME_BUF], int max) {
  int n = 0;
  if (n < max) { snprintf(codes[n], LANG_CODE_BUF, "it"); snprintf(names[n], LANG_NAME_BUF, "Italiano"); n++; }
  VosFile d = drvFsOpen(LANG_DIR);
  if (d && d.isDirectory()) {
    LangHead* h = (LangHead*)malloc(sizeof(LangHead));
    char code[LANG_CODE_BUF];
    while (h && n < max && nextLangCode(d, code)) {
      fileHead(code, *h);
      snprintf(codes[n], LANG_CODE_BUF, "%s", code); snprintf(names[n], LANG_NAME_BUF, "%.*s", LANG_NAME_BUF - 1, h->name); n++;
    }
    free(h);
  }
  if (d) d.close();
  return n;
}
