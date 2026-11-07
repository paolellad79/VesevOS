// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_shell_run.cpp
// Lancio dei programmi dalla shell, come nel vecchio DOS: "run <nome>", il solo nome scritto da solo e "which <nome>".
// Un programma e un file .vsb (script) o .vab (app). La ricerca e la stessa per tutti e tre i casi (findProgram):
//   1. un nome con una barra (prova.vsb, ./prova, /sbin/prova) e un percorso: si prova solo quello;
//   2. un nome senza barre si cerca nella cartella corrente, poi in /bin, in /sbin (solo Admin) e nelle vecchie /apps e /scripts;
//   3. se manca l'estensione si prova .vsb, .vsc (il vecchio nome), .vab e .vap (il vecchio nome).
// I comandi interni vincono SEMPRE: il lancio diretto si prova solo se la prima parola non e un comando (vedi shellExecSess).
// L'avvio vero lo fanno le stesse funzioni di "script run" e "app start" (berryStart, appStart): qui non c'e nessun controllo nuovo ne
// nessun permesso in piu (script solo Admin, app col suo who= e use=). Per ora quelle funzioni leggono solo la cartella dei programmi
// del loro tipo (script in /sbin, app in /bin) e non hanno argomenti: un file trovato altrove si dice e non si avvia.
#include "vos_shell_int.h"
#include <stdlib.h>
#include "../core/vos_ext.h"
#include "../core/vos_paths.h"
#include "../core/vos_layout.h"
#include "../packages/vos_drvcmd.h"
#include "../packages/vos_progext.h"
#include "../core/vos_pacchetto.h"
#include "../drivers/vos_drv_fs.h"

namespace {

enum Kind : uint8_t { K_SCRIPT, K_APP };

struct Found {
  Kind kind;
  char path[FS_PATH_MAX + 1];     // il file trovato (percorso pulito)
  char name[32];                  // il suo nome senza cartella e senza estensione
};

const char* const kExt[] = {VOS_EXT_SCRIPT, VOS_EXT_OLD_SCRIPT, VOS_EXT_APP_B, VOS_EXT_OLD_APP_B};   // ordine di prova quando l'estensione manca (il nome nuovo prima del vecchio)

bool endsWith(const char* s, const char* ext) {
  size_t n = strlen(s), e = strlen(ext);
  return n >= e && strcmp(s + n - e, ext) == 0;
}

// Esiste ed e un file (non una cartella) che si puo nominare? I file protetti non contano; la microSD si (se e montata).
bool eScript(const char* s) { return endsWith(s, VOS_EXT_SCRIPT) || endsWith(s, VOS_EXT_OLD_SCRIPT); }
bool eApp(const char* s) { return endsWith(s, VOS_EXT_APP_B) || endsWith(s, VOS_EXT_OLD_APP_B); }

bool isFile(const char* path) {
  if (fsProtected(path)) return false;
  VosFile f = drvFsOpen(path);
  return f && !f.isDirectory();
}

// path finisce per .vsb o .vab (o i vecchi .vsc e .vap): se il file c'e lo scrive in f
bool tryPath(const char* path, Found& f) {
  if (!isFile(path)) return false;
  f.kind = eScript(path) ? K_SCRIPT : K_APP;
  snprintf(f.path, sizeof f.path, "%s", path);
  const char* base = strrchr(path, '/');
  base = base ? base + 1 : path;
  size_t n = strlen(base) - 4;                                   // senza l'estensione (sempre di 4 caratteri)
  if (n >= sizeof f.name) n = sizeof f.name - 1;
  memcpy(f.name, base, n);
  f.name[n] = 0;
  return true;
}

// base e un percorso pulito, con o senza estensione
bool tryBase(const char* base, Found& f) {
  if (eScript(base) || eApp(base)) return tryPath(base, f);
  for (const char* e : kExt) {
    char p[FS_PATH_MAX + 8];
    int n = snprintf(p, sizeof p, "%s%s", base, e);
    if (n > 0 && n <= FS_PATH_MAX && tryPath(p, f)) return true;
  }
  return false;
}

// Cerca il programma arg (regole in cima al file). Admin: puo vedere anche /sbin e /scripts.
bool findProgram(const char* arg, const ShellSess& ss, int role, Found& f) {
  char base[FS_PATH_MAX + 1];
  if (strchr(arg, '/')) return shellResolvePath(ss.cwd, arg, base, sizeof base) && tryBase(base, f);
  const char* const dirs[] = {ss.cwd, "/bin", "/sbin", "/apps", "/scripts", "/media/sd/bin", "/media/sd/sbin"};
  for (const char* dir : dirs) {
    if (role < ROLE_ADMIN && (!strcmp(dir, "/sbin") || !strcmp(dir, "/scripts") || !strcmp(dir, "/media/sd/sbin"))) continue;
    char joined[FS_PATH_MAX * 2 + 4];
    snprintf(joined, sizeof joined, "%s/%s", strcmp(dir, "/") ? dir : "", arg);
    if (shellResolvePath("/", joined, base, sizeof base) && tryBase(base, f)) return true;
  }
  return false;
}

bool inDir(const char* path, const char* dir) {
  size_t n = strlen(dir);
  return strncmp(path, dir, n) == 0 && path[n] == '/' && !strchr(path + n + 1, '/');
}

// Il file sta nella cartella dei programmi del suo tipo, in memoria interna o sulla microSD ("/media/sd" + cartella)?
bool inProgramDir(const Found& f) {
  const char* dir = layoutPath(f.kind == K_SCRIPT ? LAY_SCRIPTS : LAY_APPS);
  char sd[FS_PATH_MAX + 1];
  snprintf(sd, sizeof sd, "/media/sd%s", dir);
  return inDir(f.path, dir) || inDir(f.path, sd);
}

// Un programma sulla microSD con lo stesso nome di uno in memoria interna: parte quello in memoria interna, quindi non si finge di avviare l'altro.
bool oscuratoDallaFlash(const Found& f) {
  if (!pathSdRel(f.path)) return false;
  char p[FS_PATH_MAX + 1];
  progPercorso(p, sizeof p, layoutPath(f.kind == K_SCRIPT ? LAY_SCRIPTS : LAY_APPS), f.name, f.kind == K_SCRIPT ? PEXT_SCRIPT : PEXT_APP_B);
  return drvFsExists(p);
}

// "run <nome> ?": cosa dichiara il programma, senza avviarlo. Stessi limiti dell'avvio: script solo Admin, app col suo who=.
void mostraDati(const Found& f, OutSink& o, int role) {
  const size_t kTesta = 2048;
  char* b = (char*)malloc(kTesta);
  if (!b) { o.println(tr("Memoria insufficiente")); return; }
  VosFile file = drvFsOpen(f.path);
  size_t n = file ? file.read((uint8_t*)b, kTesta) : 0;
  PacDati d;
  pacLeggi(std::string_view(b, n), d);
  free(b);
  if (role < ROLE_ADMIN && (f.kind == K_SCRIPT || strcmp(d.chi, "oper"))) { o.println(tr("Il tuo ruolo non permette questo comando")); return; }
  o.println(trf("Nome: %s", f.name));
  if (d.versione[0]) o.println(trf("Versione: %s", d.versione)); else o.println(tr("Versione: non dichiarata"));
  if (d.autore[0]) o.println(trf("Autore: %s", d.autore)); else o.println(tr("Autore: non dichiarato"));
  if (d.descrizione[0]) o.println(trf("Descrizione: %s", d.descrizione)); else o.println(tr("Descrizione: non dichiarata"));
}

// Avvia il programma trovato; args = il resto della riga dopo il nome ("" se non c'e)
void startFound(const Found& f, const char* args, OutSink& o, int role) {
  if (!inProgramDir(f)) {
    o.println(trf("%s: non parte da qui. Per ora gli script si avviano da %s e le app da %s", f.path, layoutPath(LAY_SCRIPTS), layoutPath(LAY_APPS)));
    return;
  }
  if (oscuratoDallaFlash(f)) { o.println(trf("%s: esiste un programma con lo stesso nome in memoria interna e parte quello: cambia nome", f.path)); return; }
  if (*args) o.println(tr("Gli argomenti non vengono ancora passati allo script o all'app."));
  if (f.kind == K_SCRIPT) {
    if (role < ROLE_ADMIN) { o.println(tr("Il tuo ruolo non permette questo comando")); return; }   // come "script run"
    shStartScript(f.name, o);
  } else {
    shStartApp(f.name, o, role);
  }
}

void cmdRun(const char* line, OutSink& o, const ShellSess& ss, int role) {
  Word nm = argAt(line, 1);
  if (nm.empty()) { cmdUsage(o, "run"); return; }
  Found f;
  if (nm.cut() || !findProgram(nm.c_str(), ss, role, f)) { o.println(trf("run: %s: non trovato", nm.c_str())); return; }
  if (argAt(line, 2) == "?") { mostraDati(f, o, role); return; }
  startFound(f, restFrom(line, 2), o, role);
}

void cmdWhich(const char* line, OutSink& o, const ShellSess& ss, int role) {
  Word nm = argAt(line, 1);
  if (nm.empty()) { cmdUsage(o, "which"); return; }
  Word low = nm;
  low.lower();
  if (!nm.cut() && !strchr(nm.c_str(), '/') && shx::cmdFind(low.c_str())) { o.println(trf("%s: comando interno", nm.c_str())); return; }   // i comandi interni vincono sempre
  if (!nm.cut() && !strchr(nm.c_str(), '/') && drvCmdHas(low.c_str())) { o.println(trf("%s: comando di un driver", nm.c_str())); return; }
  Found f;
  if (nm.cut() || !findProgram(nm.c_str(), ss, role, f)) { o.println(trf("%s: non trovato", nm.c_str())); return; }
  o.println(f.kind == K_SCRIPT ? trf("%s (script)", f.path) : trf("%s (app)", f.path));
  if (!inProgramDir(f)) o.println(tr("(non si avvia da questa cartella)"));
}

}  // namespace

bool shStartScript(const char* name, OutSink& o) {
  ErrBuf err;
  if (!berryStart(name, err)) { o.println(err.c_str()); return false; }
  o.println(trf("Script avviato: %s (script out = uscita, script stop = ferma)", name));
  return true;
}

bool shStartApp(const char* name, OutSink& o, int role) {
  ErrBuf err;
  if (!appStart(name, err, (uint8_t)role, "shell")) { o.println(err.c_str()); return false; }
  o.println(trf("App avviata: %s (app out = uscita, app stop %s = ferma)", name, name));
  return true;
}

bool shCmdRun(const Word& c, const char* line, OutSink& o, const ShellSess& ss, int role) {
  if (c == "run") cmdRun(line, o, ss, role);
  else if (c == "which") cmdWhich(line, o, ss, role);
  else return false;
  return true;
}

// Lancio diretto: la prima parola della riga non e un comando, quindi la si cerca come programma. false = non c'e niente da avviare.
bool shRunDirect(const char* line, OutSink& o, const ShellSess& ss, int role) {
  Word first = argAt(line, 0);                                   // com'e scritta (le maiuscole contano nei nomi delle cartelle)
  Found f;
  if (first.cut() || !findProgram(first.c_str(), ss, role, f)) return false;
  if (argAt(line, 1) == "?") { mostraDati(f, o, role); return true; }
  startFound(f, restFrom(line, 1), o, role);
  return true;
}
