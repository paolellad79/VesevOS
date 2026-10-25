// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_shell_complete.cpp
// Completamento col Tab (vedi vos_shell_complete.h). Le regole dei permessi sono quelle della shell (la tabella dei ruoli di vos_shell_roles.cpp): chi non puo usare un
// comando non lo vede completare, ne vede i suoi sottocomandi o i suoi percorsi. Nomi e prime parole: tabella unica dei comandi
// (vos_shell_cmdtab.cpp); regole piu a fondo e percorsi: vos_shell_complete_data.cpp.
#include "vos_shell_complete.h"
#include "vos_shell_complete_data.h"
#include "vos_shell_line.h"
#include "vos_shell_cmdtab.h"
#include "vos_shell_roles.h"
#include "vos_shell_path.h"
#include "../core/vos_common.h"
#include "../core/vos_i18n.h"
#include "../security/vos_auth.h"
using namespace shx;
#include "../core/vos_heapbuf.h"
#include <string.h>

namespace {

const size_t LINE_MAX_COMPLETE = 1024;   // come SHELL_LINE_MAX: una riga piu lunga non si completa

char lowerCh(char c) { return (c >= 'A' && c <= 'Z') ? (char)(c + 32) : c; }

// Ordine alfabetico senza guardare maiuscole e minuscole, poi per codice (come ls)
int cmpName(const char* a, const char* b) {
  const char *x = a, *y = b;
  for (; *x && *y; x++, y++) {
    char p = lowerCh(*x), q = lowerCh(*y);
    if (p != q) return (unsigned char)p < (unsigned char)q ? -1 : 1;
  }
  if (*x != *y) return *x ? 1 : -1;
  return strcmp(a, b);
}

// I candidati: nomi uno dopo l'altro nel pool del risultato, senza doppioni, con i tetti di numero e di spazio.
class Cands {
 public:
  explicit Cands(ShellCompletion& o) : o_(o) {}
  void add(const char* name, bool dir) {
    size_t n = strlen(name);
    for (int i = 0; i < o_.n; i++) {                                   // niente doppioni
      const char* s = o_.name(i);
      if (strncmp(s, name, n) == 0 && (s[n] == 0 || (dir && s[n] == '/' && s[n + 1] == 0))) return;
    }
    size_t need = n + (dir ? 1 : 0) + 1;
    if (o_.n >= SHELL_COMPL_MAX || used_ + need > SHELL_COMPL_POOL) { o_.more = true; return; }
    char* p = o_.pool + used_;
    memcpy(p, name, n);
    if (dir) p[n++] = '/';
    p[n] = 0;
    o_.off[o_.n++] = (uint16_t)used_;
    used_ += need;
  }
  void markMore() { o_.more = true; }
  // Mette in ordine i nomi e scrive in add quello che manca sulla riga: typed e la parola gia digitata (tutti i nomi cominciano cosi).
  void finish(const char* typed, size_t room) {
    for (int i = 1; i < o_.n; i++) {                                   // inserimento: al massimo 64 voci
      uint16_t v = o_.off[i];
      int j = i - 1;
      while (j >= 0 && cmpName(o_.pool + o_.off[j], o_.pool + v) > 0) { o_.off[j + 1] = o_.off[j]; j--; }
      o_.off[j + 1] = v;
    }
    o_.add[0] = 0;
    if (o_.n == 0) return;
    const char* first = o_.name(0);
    size_t common = strlen(first);
    for (int i = 1; i < o_.n; i++) {
      const char* s = o_.name(i);
      size_t m = 0;
      while (m < common && s[m] == first[m]) m++;
      common = m;
    }
    size_t tl = strlen(typed);
    size_t extra = common > tl ? common - tl : 0;
    if (extra > sizeof(o_.add) - 2) extra = sizeof(o_.add) - 2;
    memcpy(o_.add, first + tl, extra);
    if (o_.n == 1 && !o_.more && common > 0 && first[common - 1] != '/') o_.add[extra++] = ' ';   // un solo candidato: si chiude con lo spazio (le cartelle hanno gia '/')
    o_.add[extra] = 0;
    if (extra > room) o_.add[room] = 0;                                // non oltre la riga
  }

 private:
  ShellCompletion& o_;
  size_t           used_ = 0;
};

bool inList(const char* list, const char* w) {
  size_t wl = strlen(w);
  while (*list) {
    while (*list == ' ') list++;
    const char* b = list;
    while (*list && *list != ' ') list++;
    if ((size_t)(list - b) == wl && strncmp(b, w, wl) == 0) return true;
  }
  return false;
}

// Chiama add() per ogni parola della lista che comincia con prefix
template <typename F>
void eachWord(const char* list, const char* prefix, F&& fn) {
  size_t pl = strlen(prefix);
  while (*list) {
    while (*list == ' ') list++;
    const char* b = list;
    while (*list && *list != ' ') list++;
    size_t n = (size_t)(list - b);
    if (n == 0 || n < pl || strncmp(b, prefix, pl) != 0) continue;
    char w[40];
    if (n >= sizeof w) continue;
    memcpy(w, b, n);
    w[n] = 0;
    fn(w);
  }
}

// Questo ruolo puo usare il comando cmd con il sottocomando a1? Stessa tabella di shellExecSess (vos_shell_roles.cpp): nessun secondo elenco.
bool canUse(int role, const Word& cmd, const Word& a1) { return roleAllows(role, cmd, a1); }

// Aggiunge i comandi (nomi ufficiali, non nascosti) che cominciano con prefix e che questo ruolo puo usare: per la prima parola della riga
// e per "help", "man" e "time" (dopo di loro viene un nome di comando). Il nome e l'elenco vengono dalla tabella unica dei comandi.
void addCommands(int role, const char* prefix, Cands& c) {
  Word none;
  size_t pl = strlen(prefix);
  for (size_t i = 0; i < cmdCount(); i++) {
    const CmdInfo& ci = cmdAt(i);
    if ((ci.flags & CMD_HIDDEN) || strncmp(ci.name, prefix, pl) != 0) continue;
    if (canUse(role, Word(ci.name, strlen(ci.name)), none)) c.add(ci.name, false);
  }
}

// Una parola word va mostrata a questo ruolo? Alla posizione 1 e il sottocomando stesso; piu avanti conta il sottocomando gia scritto (a1).
bool subAllowed(int role, const Word& cmd, int idx, const Word& a1, const char* word) {
  return roleAllows(role, cmd, idx == 1 ? Word(word, strlen(word)) : a1);
}

// Il testo e fatto di parole che la shell sa leggere? Niente controlli, niente non ASCII, niente virgolette, apici e '\'.
bool plainLine(const char* line) {
  size_t n = 0;
  for (const char* p = line; *p; p++, n++) {
    unsigned char c = (unsigned char)*p;
    if (c < 32 || c >= 127 || c == '"' || c == '\'' || c == '\\') return false;
  }
  return n <= LINE_MAX_COMPLETE;
}

// La parte della parola dopo l'ultima '/' (quella che si sta davvero completando)
const char* baseOf(const char* word) {
  const char* slash = strrchr(word, '/');
  return slash ? slash + 1 : word;
}

bool hiddenName(const char* n) {
  size_t l = strlen(n);
  return n[0] == '.' || (l > 5 && memcmp(n + l - 5, ".tmp~", 5) == 0);
}

// Un nome si puo scrivere sulla riga? Solo caratteri visibili senza apici, virgolette, '\' e '?' (il '?' e quello che mette fsListBuild al posto dei caratteri strani)
bool typable(const char* n) {
  for (; *n; n++) {
    unsigned char c = (unsigned char)*n;
    if (c <= 32 || c >= 127 || c == '"' || c == '\'' || c == '\\' || c == '?') return false;
  }
  return true;
}

// Percorsi: word e la parola che si sta scrivendo ("scr/fo"), si elencano le voci della cartella "scr/" che cominciano con "fo".
void completePath(uint32_t sess, const char* word, bool dirsOnly, Cands& c) {
  if (!strcmp(word, "~")) { c.add("~", true); return; }                  // come bash: "~" diventa "~/"
  const char* base = baseOf(word);
  size_t dl = (size_t)(base - word);
  char dirArg[FS_PATH_MAX + 2];
  if (dl > FS_PATH_MAX) return;
  memcpy(dirArg, word, dl);
  dirArg[dl] = 0;
  ShellSess ss;
  shellSessLoad(sess, ss);
  char dir[FS_PATH_MAX + 1];
  if (!shellResolvePath(ss.cwd, dirArg, dir, sizeof dir)) return;        // prima si risolve (.. e relativi), poi si controlla
  if (fsProtected(dir)) return;                                          // niente da dire su cio che e protetto
  if (!strcmp(base, ".") || !strcmp(base, "..")) c.add("..", true);
  HeapBuf j(FS_JSON_MAX);
  if (!fsListBuild(dir, j)) return;
  const char* p = j.c_str();
  if (strncmp(p, "{\"ok\":true", 10) != 0) return;                       // cartella che manca o non valida: nessun candidato
  size_t bl = strlen(base), dirLen = strlen(dir);
  bool wantHidden = base[0] == '.';
  while ((p = strstr(p, "{\"n\":\"")) != nullptr) {
    p += 6;
    char name[FS_PATH_MAX + 2];
    size_t nl = 0;
    bool bad = false;
    while (*p && *p != '"') {                                            // il nome in JSON: ogni escape lo rende non scrivibile
      if (*p == '\\') { bad = true; if (p[1]) p++; }
      else if (nl < sizeof name - 1) name[nl++] = *p;
      else bad = true;
      p++;
    }
    name[nl] = 0;
    if (*p != '"') break;
    p++;
    bool isDir = strncmp(p, ",\"d\":true", 9) == 0;
    if (bad || !typable(name) || (dirsOnly && !isDir)) continue;
    if (dirLen + 1 + nl > FS_PATH_MAX) continue;                         // un nome che la shell non potrebbe usare
    if (strncmp(name, base, bl) != 0) continue;
    if (hiddenName(name) && !wantHidden) continue;
    c.add(name, isDir);
  }
  if (strstr(j.c_str(), "],\"more\":true")) c.markMore();   // elenco troncato da fsListBuild
}

}  // namespace

bool shellComplete(uint32_t sess, int role, const char* line, size_t room, bool web, ShellCompletion& out) {
  out.reset();
  if (!line || !plainLine(line)) return false;
  if (role < 0) { if (authIsSet()) return false; role = ROLE_ADMIN; }   // come shellExecSess
  if (role < ROLE_OPER) return false;                                    // l'Ospite non ha comandi

  size_t n = strlen(line), b = n;
  while (b > 0 && line[b - 1] != ' ') b--;
  const char* word = line + b;                                           // la parola che si sta scrivendo (puo essere vuota)
  if (n - b > FS_PATH_MAX) return false;
  int idx = 0;                                                           // parole intere prima di questa
  for (const char* s = line; s < word;) {
    while (s < word && *s == ' ') s++;
    if (s < word) { idx++; while (s < word && *s != ' ') s++; }
  }

  Cands c(out);
  if (idx == 0) {
    if (!*word) return false;                                            // riga vuota: niente (sono troppi comandi)
    addCommands(role, word, c);
    if (web) { if (strncmp("clear", word, strlen(word)) == 0) c.add("clear", false); if (strncmp("history", word, strlen(word)) == 0) c.add("history", false); }
    c.finish(word, room);
    return out.n > 0;
  }

  Word cmd = argAt(line, 0), a1 = argAt(line, 1);
  if (cmd.cut() || a1.cut()) return false;
  cmd.lower();
  canonicalName(cmd);                                                    // i vecchi nomi (aereo, licenza...) si completano come quelli ufficiali
  if (!canUse(role, cmd, idx == 1 ? Word() : a1)) return false;          // comando vietato a questo ruolo: nessun suggerimento

  for (size_t i = 0; i < kPathRulesN; i++) {                             // comandi con percorsi
    const PathRule& r = kPathRules[i];
    if (cmd != r.cmd || idx < r.first || idx > r.last) continue;
    bool opt = (r.opts && word[0] == '-');                                 // ls -l, gzip -d, tar -xf...: opzioni, non percorsi
    if (opt) eachWord(r.opts, word, [&](const char* w) { c.add(w, false); });
    else completePath(sess, word, r.dirsOnly, c);
    c.finish(opt ? word : baseOf(word), room);
    return out.n > 0;
  }

  if (idx == 2 && cmd == "vesevos-backup" && (a1 == "restore" || a1 == "delete")) {   // i nomi dei backup: i file di /backup (solo per restore e delete)
    char full[FS_PATH_MAX + 1];
    if (strlen(word) + 9 > FS_PATH_MAX) return false;
    snprintf(full, sizeof full, "/backup/%s", word);
    completePath(sess, full, false, c);
    c.finish(word, room);
    return out.n > 0;
  }

  if (idx == 1) {                                                        // la prima parola dopo il comando: dalla tabella unica dei comandi
    const CmdInfo* ci = cmdFind(cmd.c_str());
    if (ci && strcmp(ci->subs, "@commands") == 0) addCommands(role, word, c);
    else if (ci && ci->subs[0]) eachWord(ci->subs, word, [&](const char* w) { if (subAllowed(role, cmd, 1, a1, w)) c.add(w, false); });
  }

  Word prev = argAt(line, idx - 1);                                      // piu a fondo: regole con contesto (vos_shell_complete_data.cpp)
  if (prev.cut()) return false;
  for (size_t i = 0; i < kSubRulesN; i++) {
    const SubRule& r = kSubRules[i];
    if (cmd != r.cmd || r.idx != idx || (r.prev && !inList(r.prev, prev.c_str()))) continue;
    eachWord(r.words, word, [&](const char* w) { if (subAllowed(role, cmd, idx, a1, w)) c.add(w, false); });
  }
  c.finish(word, room);
  return out.n > 0;
}

void shellCompletePrintList(OutSink& o, const ShellCompletion& c) {
  size_t w = 0;
  for (int i = 0; i < c.n; i++) { size_t l = strlen(c.name(i)); if (l > w) w = l; }
  w += 2;
  size_t cols = 78 / w;
  if (cols == 0) cols = 1;
  size_t rows = ((size_t)c.n + cols - 1) / cols;
  for (size_t r = 0; r < rows; r++) {                                    // come bash: le colonne si riempiono dall'alto in basso
    for (size_t k = 0; k < cols; k++) {
      size_t i = k * rows + r;
      if (i >= (size_t)c.n) break;
      const char* s = c.name((int)i);
      o.print(s);
      if (i + rows < (size_t)c.n) for (size_t pad = strlen(s); pad < w; pad++) o.print(' ');
    }
    o.println();
  }
  if (c.more) o.println("...");
}
