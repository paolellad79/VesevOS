// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_mod_cmd.cpp (vedi vos_mod_cmd.h): righe [commands], codice [cmd] e [widget] dei driver in /devs.
#include "vos_mod_cmd.h"
#include <string.h>
#include "../core/vos_files.h"
#include "../core/vos_i18n.h"
#include "../core/vos_util.h"
#include "../core/vos_vdesc.h"

// Le sezioni che interessano qui. Dentro [cmd] (codice) una riga tra parentesi quadre che non e una sezione nota resta codice.
enum SecKind { SEC_OTHER, SEC_COMMANDS, SEC_CMD };
static bool isHeader(std::string_view t, SecKind& k) {            // true = t e l'intestazione di una sezione nota (k diventa quella sezione)
  if (t.size() < 3 || t[0] != '[' || t.back() != ']') return false;
  std::string_view s = t.substr(1, t.size() - 2);
  if (!modSectionKnown(s)) return false;
  k = viewEqI(s, "commands") ? SEC_COMMANDS : viewEqI(s, "cmd") ? SEC_CMD : SEC_OTHER;
  return true;
}

int modCmdList(ModCmdEntry* out, int mx) {
  char (*names)[MOD_NAME_BUF] = new char[MOD_LIST_MAX][MOD_NAME_BUF];
  int total = modScan(names, MOD_LIST_MAX), n = total < MOD_LIST_MAX ? total : MOD_LIST_MAX, got = 0;
  for (int i = 0; i < n && got < mx; i++) {
    if (!utilNameOk(names[i], strlen(names[i]))) continue;
    char path[64]; modFilePath(path, sizeof path, names[i]);
    HeapBuf text(fsTextMax()); ErrBuf e;
    if (!fsReadText(path, text, e)) continue;
    SecKind k = SEC_OTHER; int rows = 0;
    LineIter it(std::string_view(text.c_str(), text.length())); std::string_view raw;
    while (it.next(raw) && got < mx) {
      std::string_view t = trimView(raw);
      if (isHeader(t, k) || k != SEC_COMMANDS || t.empty() || t[0] == '#' || rows >= MOD_CMD_MAX) continue;
      ModCmd c; const char* why;
      if (!modCmdLine(t, c, &why)) continue;
      snprintf(out[got].driver, sizeof out[got].driver, "%s", names[i]);
      out[got].c = c; got++; rows++;
    }
  }
  delete[] names;
  return got;
}

bool modCmdCode(const char* driver, HeapBuf& code, char* use, size_t useCap, ErrBuf& err) {
  if (!utilNameOk(driver, strlen(driver))) { err = tr("Nome non valido"); return false; }
  char path[64]; modFilePath(path, sizeof path, driver);
  HeapBuf text(fsTextMax()); ErrBuf e;
  if (!fsReadText(path, text, e)) { err = e.empty() ? tr("File non trovato") : e.c_str(); return false; }
  std::string_view all(text.c_str(), text.length());
  ModInfo* m = new ModInfo();
  bool ok = modParse(all, driver, *m, err);
  if (ok && !m->hasCmd) { err = tr("Il driver non ha comandi"); ok = false; }
  if (ok) snprintf(use, useCap, "%s", m->use);
  delete m;
  if (!ok) return false;
  code.clear();
  SecKind k = SEC_OTHER;
  LineIter it(all); std::string_view raw;
  while (it.next(raw)) {
    if (isHeader(trimView(raw), k)) continue;
    if (k == SEC_CMD) { code.add(raw.data(), raw.size()); code.add('\n'); }
  }
  if (code.oom() || code.over()) { err = tr("Memoria insufficiente"); return false; }
  return true;
}

int modWidgetList(ModWidgetEntry* out, int mx) {
  char (*names)[MOD_NAME_BUF] = new char[MOD_LIST_MAX][MOD_NAME_BUF];
  int total = modScan(names, MOD_LIST_MAX), n = total < MOD_LIST_MAX ? total : MOD_LIST_MAX, got = 0;
  ModInfo* m = new ModInfo();
  for (int i = 0; i < n && got < mx; i++) {
    if (!utilNameOk(names[i], strlen(names[i]))) continue;
    char path[64]; modFilePath(path, sizeof path, names[i]);
    HeapBuf text(fsTextMax()); ErrBuf e;
    if (!fsReadText(path, text, e)) continue;
    if (!modParse(std::string_view(text.c_str(), text.length()), names[i], *m, e) || !m->widget.on) continue;
    snprintf(out[got].driver, sizeof out[got].driver, "%s", names[i]);
    out[got].w = m->widget;
    out[got].source[0] = 0;
    snprintf(out[got].icon, sizeof out[got].icon, "%s", m->icon);
    got++;
  }
  delete m;
  delete[] names;
  return got;
}
