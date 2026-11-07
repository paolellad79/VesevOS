// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_mod_app.cpp (vedi vos_mod_app.h): dal driver .vdb alla sua app .vab.
#include "vos_mod_app.h"
#include "../packages/vos_progext.h"
#include <string.h>
#include <strings.h>
#include "../core/vos_files.h"
#include "../core/vos_i18n.h"
#include "../core/vos_log.h"
#include "../core/vos_pacchetto.h"
#include "../packages/vos_pacchetto_nome.h"
#include "../core/vos_util.h"
#include "../core/vos_vdesc.h"
#include "../devices/vos_mod.h"
#include "../packages/vos_app.h"
#include "../packages/vos_drvcmd.h"
#include "../packages/vos_drvwidget.h"
#include "vos_shell_cmdtab.h"

static bool isSec(std::string_view t, const char* name) { return t.size() > 2 && t[0] == '[' && t.back() == ']' && viewEqI(t.substr(1, t.size() - 2), name); }

// true se il driver ha una sezione [code] (anche vuota)
static bool hasCode(std::string_view vdp) {
  LineIter it(vdp); std::string_view raw;
  while (it.next(raw)) if (isSec(trimView(raw), "code")) return true;
  return false;
}

#if VOS_WITH_BERRY

#define MARK "# driver:"                                  // prima riga del file generato

// Costruisce il testo della app: [manifest] dalle chiavi di [module], poi [icon] [page] [code] [data] copiate. Il resto del driver non entra.
static bool buildApp(const char* name, std::string_view vdp, HeapBuf& vap, ErrBuf& err) {
  vap.add(MARK " "); vap.add(name); vap.add(" - generato: cambia il driver in /devs e installalo di nuovo\n[manifest]\nname="); vap.add(name); vap.add('\n');
  enum { OTHER, MOD, ICO, PAG, COD, DAT } sec = OTHER;
  LineIter it(vdp); std::string_view raw;
  HeapBuf body(MOD_FILE_MAX);                             // le sezioni dopo il manifest, nell'ordine del file
  while (it.next(raw)) {
    std::string_view t = trimView(raw);
    bool br = t.size() > 1 && t[0] == '[' && t.back() == ']';
    if (br && (sec != COD || modSectionKnown(t.substr(1, t.size() - 2)))) {
      sec = isSec(t, "module") ? MOD : isSec(t, "icon") ? ICO : isSec(t, "page") ? PAG : isSec(t, "code") ? COD : isSec(t, "data") ? DAT : OTHER;
      if (sec >= ICO) { body.add(t.data(), t.size()); body.add('\n'); }
      continue;
    }
    if (sec == COD || (sec >= ICO && !t.empty() && t[0] != '#')) { body.add(raw.data(), raw.size()); body.add('\n'); continue; }
    if (sec != MOD || t.empty() || t[0] == '#') continue;
    size_t q = t.find('=');
    if (q == std::string_view::npos) continue;
    std::string_view k = pacChiave(trimView(t.substr(0, q))), v = trimView(t.substr(q + 1));
    if (viewEqI(k, "title")) v = v.substr(0, 24);         // il titolo di una app ha al massimo 24 caratteri
    if (viewEqI(k, "title") || viewEqI(k, "version") || viewEqI(k, "author") || viewEqI(k, "use") || viewEqI(k, "who") || viewEqI(k, "autostart")) {
      vap.add(k.data(), k.size()); vap.add('='); vap.add(v.data(), v.size()); vap.add('\n');
    }
  }
  vap.add(body.c_str(), body.length());
  if (vap.oom() || vap.over() || body.oom() || body.over()) { err = tr("Memoria insufficiente"); return false; }
  return true;
}

bool modAppCheck(const char* name, std::string_view vdp, ErrBuf& err) {
  if (!hasCode(vdp)) return true;
  HeapBuf vap(MOD_FILE_MAX + 1024);
  if (!buildApp(name, vdp, vap, err)) return false;
  AppInfo* a = new AppInfo(); ErrBuf e;
  bool ok = appParse(std::string_view(vap.c_str(), vap.length()), name, *a, e);
  delete a;
  if (!ok) { char msg[200]; snprintf(msg, sizeof msg, "%s: %s", tr("App del driver"), e.c_str()); err = msg; }
  return ok;
}

// Dove sta l'app del driver: il nome nuovo (.vab) se c'e, altrimenti il vecchio (.vap); se non c'e nessuno dei due, il nuovo (quello che si scrive).
static void appPath(const char* name, char* out, size_t cap) { progPercorso(out, cap, APP_DIR, name, PEXT_APP_B); }

// true se il file esiste ed e stato generato da un driver
static bool generated(const char* path) {
  HeapBuf t(fsTextMax()); ErrBuf e;
  return fsReadText(path, t, e) && !strncmp(t.c_str(), MARK, strlen(MARK));
}
static bool exists(const char* path) { HeapBuf t(fsTextMax()); ErrBuf e; return fsReadText(path, t, e); }

bool modAppInstall(const char* name, ErrBuf& err) {
  HeapBuf text(fsTextMax()); ErrBuf e;
  char path[64];
  modFilePath(path, sizeof path, name);
  if (!fsReadText(path, text, e)) { err = e.empty() ? tr("File non trovato") : e.c_str(); return false; }
  std::string_view vdp(text.c_str(), text.length());
  if (!hasCode(vdp)) return true;
  if (!modAppCheck(name, vdp, err)) return false;
  HeapBuf vap(MOD_FILE_MAX + 1024);
  if (!buildApp(name, vdp, vap, err)) return false;
  appPath(name, path, sizeof path);
  if (exists(path) && !generated(path)) { err = tr("Esiste gia un'app con questo nome"); return false; }
  snprintf(path, sizeof path, "%s/%s%s", APP_DIR, name, PEXT_APP_B.nuovo);                  // si scrive sempre il nome nuovo
  ErrBuf e0; fsMkdir(APP_DIR, e0);
  if (!fsWriteText(path, std::string_view(vap.c_str(), vap.length()), err, false)) return false;
  char old[64]; snprintf(old, sizeof old, "%s/%s%s", APP_DIR, name, PEXT_APP_B.vecchio);
  if (generated(old)) progToglieVecchio(APP_DIR, name, PEXT_APP_B);                             // il vecchio .vap generato dallo stesso driver non serve piu
  vlog("MOD: app del driver %s scritta in %s", name, path);
  return true;
}

void modAppRemove(const char* name) {
  char path[64]; ErrBuf e;
  snprintf(path, sizeof path, "%s/%s%s", APP_DIR, name, PEXT_APP_B.nuovo);
  if (generated(path)) fsRemove(path, e);
  snprintf(path, sizeof path, "%s/%s%s", APP_DIR, name, PEXT_APP_B.vecchio);                  // anche l'app scritta dalle versioni con il nome vecchio
  if (generated(path)) fsRemove(path, e);
}

static bool modAppExists(const char* name) { char p[64]; appPath(name, p, sizeof p); return generated(p); }
#else   // senza Berry un driver con [code] non si puo installare; senza [code] funziona come una scheda modulo
bool modAppCheck(const char*, std::string_view vdp, ErrBuf& err) { if (!hasCode(vdp)) return true; err = tr("Questo firmware e senza script"); return false; }
bool modAppInstall(const char* name, ErrBuf& err) {
  HeapBuf text(fsTextMax()); ErrBuf e;
  char path[64];
  modFilePath(path, sizeof path, name);
  if (!fsReadText(path, text, e)) { err = e.empty() ? tr("File non trovato") : e.c_str(); return false; }
  return modAppCheck(name, std::string_view(text.c_str(), text.length()), err);
}
void modAppRemove(const char*) {}
static bool modAppExists(const char*) { return false; }
#endif

// Un driver non puo prendere il nome di un comando interno: i comandi interni vincono sempre, quello del driver non partirebbe mai.
static bool cmdWordFree(std::string_view text, const char* name, ErrBuf& err) {
  ModInfo* m = new ModInfo(); ErrBuf e;
  bool ok = modParse(text, name, *m, e);                  // se il file non e valido lo dice poi modSave, con la riga giusta
  bool clash = ok && m->hasCmd && shx::cmdFind(m->cmdWord);
  if (clash) { char msg[120]; snprintf(msg, sizeof msg, "%s: %s", tr("Comando gia usato dal sistema"), m->cmdWord); err = msg; }
  delete m;
  return !clash;
}

bool modInstallDriver(const char* name, const char* prefix, const char* pinsCsv, HeapBuf& outJson, ErrBuf& err) {
  bool had = modAppExists(name);
  HeapBuf text(fsTextMax()); ErrBuf e; char path[64];
  modFilePath(path, sizeof path, name);
  if (fsReadText(path, text, e) && !cmdWordFree(std::string_view(text.c_str(), text.length()), name, err)) return false;
  if (!modAppInstall(name, err)) return false;
  if (modInstall(name, prefix, pinsCsv, outJson, err)) { drvCmdRefresh(); drvWidgetRefresh(); return true; }
  if (!had) modAppRemove(name);                           // le periferiche non si sono create: l'app non resta da sola
  return false;
}

bool modSaveDriver(const char* nomeCaricato, std::string_view text, ErrBuf& err, char* salvato, size_t cap, ErrBuf* avviso) {
  PacDati d;
  pacLeggi(text, d);
  const char* name = d.nome[0] ? d.nome : nomeCaricato;   // il nome corto dichiarato nel file vale piu del nome del file caricato
  if (!utilNameOk(name, strlen(name))) { err = tr("Nome non valido"); return false; }
  if (!modAppCheck(name, text, err)) return false;
  if (!cmdWordFree(text, name, err)) return false;
  ErrBuf avvisoVersione;
  char installato[64];
  modFilePath(installato, sizeof installato, name);
  pacchettoAvviso(installato, text, avvisoVersione);      // si guarda prima: dopo il salvataggio il vecchio non c'e piu
  if (!modSave(name, text, err)) return false;
  if (avviso) *avviso = avvisoVersione.c_str();
  if (salvato) snprintf(salvato, cap, "%s", name);
  drvCmdRefresh();
  drvWidgetRefresh();
  return true;
}

bool modDeleteDriver(const char* name, ErrBuf& err) {
  if (!modDelete(name, err)) return false;
  modAppRemove(name);
  drvCmdRefresh();
  drvWidgetRefresh();
  return true;
}
