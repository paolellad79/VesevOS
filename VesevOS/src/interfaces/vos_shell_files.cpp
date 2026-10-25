// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_shell_files.cpp
// Comandi della shell sui file di LittleFS: pwd, cd, ls, cat, mkdir, rm, mv, cp, write. Tutti i percorsi passano da
// shellResolvePath (vos_shell_path.h): assoluti come sempre, relativi alla cartella corrente della sessione (vos_shell_sess.h).
// Le protezioni (file di sistema, nomi, limiti) restano nelle funzioni di vos_files, che ricevono sempre il percorso gia risolto.
// Questi comandi non sono tra quelli dell'Operatore (vedi la tabella in vos_shell_roles.cpp): qui arriva solo l'Admin, come prima.
#include "vos_shell_int.h"
#include "../core/vos_heapbuf.h"

namespace {

// Percorso risolto in out (FS_PATH_MAX + 1). Se non e valido scrive l'errore e ritorna false.
bool resolve(const ShellSess& ss, const Word& arg, char* out, OutSink& o) {
  if (arg.cut() || !shellResolvePath(ss.cwd, arg.c_str(), out, FS_PATH_MAX + 1)) { o.println(tr("Percorso non valido")); return false; }
  return true;
}

// "." e ".." (anche in fondo a un percorso): rm e mv non li accettano, come in una shell classica
bool isDotName(const char* s) {
  size_t end = strlen(s);
  while (end > 1 && s[end - 1] == '/') end--;               // barre finali
  size_t start = end;
  while (start && s[start - 1] != '/') start--;             // ultimo pezzo del percorso
  size_t len = end - start;
  return (len == 1 && s[start] == '.') || (len == 2 && s[start] == '.' && s[start + 1] == '.');
}

void cmdPwd(OutSink& o, const ShellSess& ss) { o.println(ss.cwd); }

void cmdCd(const Word& a1, const char* line, OutSink& o, ShellSess& ss) {
  if (!argAt(line, 2).empty()) { o.println(tr("cd: troppi argomenti")); return; }
  char dest[FS_PATH_MAX + 1];
  if (a1 == "-") {
    snprintf(dest, sizeof dest, "%s", ss.prev);
  } else if (a1.empty()) {
    dest[0] = '/'; dest[1] = 0;
  } else if (!resolve(ss, a1, dest, o)) return;
  if (fsProtected(dest)) { o.println(trf("cd: %s: Permesso negato", a1.c_str())); return; }
  VosFile d = drvFsOpen(dest);                              // aprire basta: nessun elenco, nessun calcolo di dimensioni
  if (!d) { o.println(trf("cd: %s: Non esiste", a1.c_str())); return; }
  if (!d.isDirectory()) { o.println(trf("cd: %s: Non e una cartella", a1.c_str())); return; }
  snprintf(ss.prev, sizeof ss.prev, "%s", ss.cwd);
  snprintf(ss.cwd, sizeof ss.cwd, "%s", dest);
  if (a1 == "-") o.println(ss.cwd);                         // come in una shell classica: "cd -" dice dove e arrivato
}

}  // namespace

bool shCmdFiles(const Word& c, const Word& a1, const char* line, OutSink& o, int role, ShellSess& ss) {
  (void)role;
  char p[FS_PATH_MAX + 1], p2[FS_PATH_MAX + 1];
  ErrBuf err;
  if (c == "pwd") cmdPwd(o, ss);
  else if (c == "cd") cmdCd(a1, line, o, ss);
  else if (c == "ls") shLs(line, o, ss);
  else if (c == "cat") {
    if (a1.empty()) { o.println(tr("Uso: cat <file>")); return true; }
    if (!resolve(ss, a1, p, o)) return true;
    HeapBuf t(fsTextMax());
    if (!fsReadText(p, t, err)) { o.println(err.c_str()); return true; }
    o.println(t.c_str());
  }
  else if (c == "mkdir") {
    if (a1.empty()) { o.println(tr("Uso: mkdir <cartella>")); return true; }
    if (!resolve(ss, a1, p, o)) return true;
    if (fsMkdir(p, err)) o.println(tr("Creata")); else o.println(err.c_str());
  }
  else if (c == "rm") {
    if (a1.empty()) { o.println(tr("Uso: rm <file o cartella>")); return true; }
    if (isDotName(a1.c_str())) { o.println(tr("Non si possono eliminare '.' e '..'")); return true; }
    if (!resolve(ss, a1, p, o)) return true;
    if (fsRemove(p, err)) o.println(tr("Eliminato")); else o.println(err.c_str());
  }
  else if (c == "mv" || c == "cp") {
    Word a2 = argAt(line, 2);
    if (a1.empty() || a2.empty()) { o.println(trf("Uso: %s <da> <a>", c.c_str())); return true; }
    if (c == "mv" && isDotName(a1.c_str())) { o.println(tr("Non si possono spostare '.' e '..'")); return true; }
    if (!resolve(ss, a1, p, o) || !resolve(ss, a2, p2, o)) return true;
    bool r = (c == "mv") ? fsRename(p, p2, err) : fsCopy(p, p2, err);
    if (r) o.println(tr("Fatto")); else o.println(err.c_str());
  }
  else if (c == "write") {
    if (a1.empty()) { o.println(tr("Uso: write <file> <testo>")); return true; }
    if (!resolve(ss, a1, p, o)) return true;
    if (fsWriteText(p, restFrom(line, 2), err)) o.println(tr("Scritto")); else o.println(err.c_str());
  }
  else return false;
  return true;
}
