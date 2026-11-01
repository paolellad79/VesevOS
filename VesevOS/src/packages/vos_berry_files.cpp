// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_berry_files.cpp: funzioni Berry per LEGGERE i metadati di cartelle e file (memoria interna e microSD).
//
//   listdir("percorso" [, indice])  ->  {"items": [voce, ...], "more": bool, "next": int}
//   stat("percorso")                ->  voce, oppure nil se non c'e (o non lo puoi vedere)
//   voce = {"name": testo, "dir": bool, "size": byte (0 per le cartelle), "mtime": secondi dal 1970, 0 = data non disponibile}
//
// REGOLE
//   - Il percorso passa da core/vos_pathres (pathResolve, partendo dalla radice): "..", "/sd" come scorciatoia di "/media/sd", ecc.
//     "/media" si elenca (contiene "sd" se la scheda e montata). La memoria interna non salva le date: mtime e 0.
//   - Al massimo VOL_PAGE_MAX voci per chiamata. Se more e true si richiama con indice = next (indice massimo VOL_START_MAX).
//   - I file di sistema non si vedono mai; chiavi, certificati e copie .vbk solo se lo script e stato lanciato dall'Admin.
//     Le app non sono mai "Admin" per questo controllo.
//   - Permesso: gli script (.vsb e "berry <codice>") le hanno, come logtail e mqtt; le app solo con "files" in use= del manifest.
//   - Non apre il contenuto dei file e non scrive niente. Nessun oggetto con distruttore resta vivo quando si chiama be_raise (longjmp).
#include "vos_berry_int.h"
#include "vos_func.h"
#include "../core/vos_common.h"
#if VOS_WITH_BERRY
#include "../third_party/berry/src/berry.h"
#include <string.h>
#include "../core/vos_files.h"
#include "../core/vos_pathres.h"
#include "../devices/vos_vol.h"

static VolEntry g_ents[VOL_PAGE_MAX];       // una sola esecuzione di Berry alla volta: un solo elenco in uso

static int callerRole() { return g_admin ? ROLE_ADMIN : ROLE_OPER; }

static const char* errText(VolErr e) {
  switch (e) {
    case VOL_BAD_ARG:   return "parametri non validi";
    case VOL_BAD_PATH:  return "percorso non ammesso sulla microSD";
    case VOL_NOT_FOUND: return "non trovato";
    case VOL_NOT_DIR:   return "non e una cartella";
    case VOL_NO_MEDIA:  return "la microSD non e montata";
    case VOL_TOO_LONG:  return "nome troppo lungo";
    case VOL_NO_MEMORY: return "memoria insufficiente";
    default:            return "errore";
  }
}

// Il percorso scritto dallo script, risolto dalla radice. Errore Berry se non e valido.
static void pathArg(bvm* vm, const char* fn, char* out) {
  if (be_top(vm) < 1 || !be_isstring(vm, 1)) {
    be_raise(vm, "type_error", strcmp(fn, "stat") == 0 ? "stat(\"percorso\")" : "listdir(\"percorso\" [, indice])");
  }
  if (!pathResolve("/", be_tostring(vm, 1), out, FS_PATH_MAX + 1)) be_raise(vm, "value_error", "percorso non valido");
}

// Mette in cima alla pila la mappa chiave -> valore (la mappa e sotto i due valori appena messi). Le usa anche vos_berry_sd.cpp.
void berryPutInt(bvm* vm, const char* k, long v) { be_pushstring(vm, k); be_pushint(vm, (bint)v); be_data_insert(vm, -3); be_pop(vm, 2); }
void berryPutBool(bvm* vm, const char* k, bool v) { be_pushstring(vm, k); be_pushbool(vm, v); be_data_insert(vm, -3); be_pop(vm, 2); }
void berryPutText(bvm* vm, const char* k, const char* v) { be_pushstring(vm, k); be_pushstring(vm, v); be_data_insert(vm, -3); be_pop(vm, 2); }

// Trasforma la mappa o lista grezza in cima alla pila in un oggetto Berry "map" o "list": solo cosi lo script puo usare m['k'] e l[i].
void berryWrap(bvm* vm, const char* cls) {
  be_getbuiltin(vm, cls);
  be_pushvalue(vm, -2);
  be_call(vm, 1);
  be_moveto(vm, -2, -3);
  be_pop(vm, 2);
}

// Funzionalita tolta dall'utente: la funzione resta registrata (un file che la nomina compila lo stesso) e risponde con un errore chiaro.
void berryNeedFunc(bvm* vm, const char* group) {
  if (funcOn(group)) return;
  char msg[64];
  snprintf(msg, sizeof msg, "funzionalita %s spenta (Sistema > Funzionalita)", group);
  be_raise(vm, "permission_error", msg);
}

// Una voce come mappa, lasciata in cima alla pila.
static void pushEntry(bvm* vm, const VolEntry& e) {
  be_newmap(vm);
  berryPutText(vm, "name", e.name);
  berryPutBool(vm, "dir", e.dir);
  berryPutInt(vm, "size", (long)e.size);
  berryPutInt(vm, "mtime", (long)e.mtime);
  berryWrap(vm, "map");
}

static int bfListdir(bvm* vm) {
  char path[FS_PATH_MAX + 1];
  pathArg(vm, "listdir", path);
  bint start = 0;
  if (be_top(vm) >= 2) {
    if (!be_isint(vm, 2)) be_raise(vm, "type_error", "listdir: l'indice deve essere un numero intero");
    start = be_toint(vm, 2);
    if (start < 0 || start > VOL_START_MAX) be_raise(vm, "value_error", "listdir: indice fuori limite (0-2000)");
  }
  int count = 0, consumed = 0;
  bool more = false;
  VolErr er = volList(path, callerRole(), (int)start, g_ents, VOL_PAGE_MAX, count, consumed, more);
  if (er != VOL_OK) be_raise(vm, "io_error", errText(er));
  if (g_stop) be_raise(vm, "stopped_error", "script fermato");
  be_newmap(vm);
  be_pushstring(vm, "items");
  be_newlist(vm);
  for (int i = 0; i < count; i++) {
    pushEntry(vm, g_ents[i]);
    be_data_push(vm, -2);                                    // nella lista (che sta sotto la mappa della voce)
    be_pop(vm, 1);
  }
  berryWrap(vm, "list");
  be_data_insert(vm, -3);                                    // "items" -> lista, nella mappa del risultato
  be_pop(vm, 2);
  berryPutBool(vm, "more", more);
  berryPutInt(vm, "next", (long)start + consumed);
  berryWrap(vm, "map");
  be_return(vm);
}

static int bfStat(bvm* vm) {
  char path[FS_PATH_MAX + 1];
  pathArg(vm, "stat", path);
  VolEntry e;
  VolErr er = volStat(path, callerRole(), e);
  if (er == VOL_NOT_FOUND) be_return_nil(vm);
  if (er != VOL_OK) be_raise(vm, "io_error", errText(er));
  pushEntry(vm, e);
  be_return(vm);
}

void berryFilesRegister(bvm* vm) {
  be_regfunc(vm, "listdir", bfListdir);
  be_regfunc(vm, "stat", bfStat);
}
#else
struct bvm;
void berryFilesRegister(bvm*) {}
#endif
