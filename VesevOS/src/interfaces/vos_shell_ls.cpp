// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_shell_ls.cpp
// Comando "ls" della shell, in stile shell classica. Legge le cartelle SOLO con fsListBuild (vos_files), che nasconde i file di
// sistema: qui non si apre mai un file per conto proprio. Aiuti e include: vos_shell_int.h.
//
//   ls [-l] [-a] [-h] [-1] [-F] [percorso ...]
//   -l formato lungo: tipo (d cartella, - file), dimensione in byte, nome. La data non c'e: LittleFS non la salva.
//      Per una cartella la dimensione e il totale dei file che contiene.
//   -a mostra anche i nomi che cominciano con '.' e i residui "nome.tmp~" (di solito nascosti)
//   -h dimensioni leggibili (K, M)       -1 una voce per riga (e gia cosi: la sigla c'e per chi la scrive per abitudine)
//   -F accettata e ignorata: le cartelle hanno SEMPRE la barra finale
// Ordine: alfabetico senza guardare maiuscole e minuscole (poi, a pari nome, per codice del carattere).
#include "vos_shell_int.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

namespace {

constexpr int LS_MAX   = 200;      // voci lette da una cartella (quanto ne da fsListBuild: oltre dice more)
constexpr int LS_SHOW  = 100;      // voci stampate per cartella (la pagina web taglia l'uscita a 6000 caratteri)
constexpr int LS_PATHS = 8;        // percorsi in una sola riga

struct LsOpt { bool all = false, lng = false, human = false; };

struct Ent {
  char*    name;                   // dentro il JSON di fsListBuild, gia senza le sequenze di escape
  uint16_t len;
  uint32_t size;
  bool     dir;
};

// Le voci di una cartella lette dal JSON di fsListBuild. Il JSON deve restare vivo finche si usano le voci.
class EntList {
 public:
  EntList() : e_((Ent*)malloc(sizeof(Ent) * LS_MAX)) {}
  ~EntList() { free(e_); }
  EntList(const EntList&) = delete;
  EntList& operator=(const EntList&) = delete;
  bool ok() const { return e_ != nullptr; }
  int  count() const { return n_; }
  bool more() const { return more_; }
  Ent& at(int i) { return e_[i]; }
  void sort() { qsort(e_, (size_t)n_, sizeof(Ent), cmp); }
  // js e il JSON {"ok":true,"list":[{"n":"..","d":false,"s":12},...],"more":false,...}; false se non e un elenco
  bool parse(char* js) {
    if (strncmp(js, "{\"ok\":true", 10) != 0) return false;
    char* p = js;
    while (n_ < LS_MAX && (p = strstr(p, "{\"n\":\"")) != nullptr) {
      char* nm = p + 6;
      char* e = strstr(nm, "\",\"d\":");
      if (!e) break;
      char* sp = strstr(e, "\"s\":");
      if (!sp) break;
      char* end = nullptr;
      Ent& x = e_[n_++];
      x.dir = strncmp(e + 6, "true", 4) == 0;
      x.size = (uint32_t)strtoul(sp + 4, &end, 10);
      x.name = nm;
      x.len = unescape(nm, (size_t)(e - nm));
      p = end ? end : sp + 4;
    }
    more_ = strstr(p ? p : js, "\"more\":true") != nullptr || n_ >= LS_MAX;
    return true;
  }

 private:
  // Toglie le sequenze \" \\ \n \r \t scritte da addJson (le ultime tre diventano '?': a capo nel nome sporcherebbero l'uscita)
  static uint16_t unescape(char* s, size_t n) {
    size_t w = 0;
    for (size_t i = 0; i < n; i++) {
      char c = s[i];
      if (c == '\\' && i + 1 < n) { c = s[++i]; if (c == 'n' || c == 'r' || c == 't') c = '?'; }
      s[w++] = c;
    }
    s[w] = 0;
    return (uint16_t)w;
  }
  static char low(char c) { return (c >= 'A' && c <= 'Z') ? (char)(c + 32) : c; }
  static int cmp(const void* pa, const void* pb) {
    const Ent& a = *(const Ent*)pa;
    const Ent& b = *(const Ent*)pb;
    size_t m = a.len < b.len ? a.len : b.len;
    for (size_t i = 0; i < m; i++) { char x = low(a.name[i]), y = low(b.name[i]); if (x != y) return (unsigned char)x < (unsigned char)y ? -1 : 1; }
    if (a.len != b.len) return a.len < b.len ? -1 : 1;
    return memcmp(a.name, b.name, m);                       // stesso nome a meno delle maiuscole: per codice
  }

  Ent* e_;
  int  n_ = 0;
  bool more_ = false;
};

bool hidden(const Ent& e) {
  if (e.len && e.name[0] == '.') return true;
  return e.len > 5 && memcmp(e.name + e.len - 5, ".tmp~", 5) == 0;
}

// 1234 -> "1234"; con human: 1536 -> "1.5K", 2097152 -> "2.0M" (una cifra decimale sotto 10, arrotondato)
void fmtSize(char* b, size_t cap, uint32_t v, bool human) {
  if (!human || v < 1024) { snprintf(b, cap, "%lu", (unsigned long)v); return; }
  uint32_t unit = v >= 1048576UL ? 1048576UL : 1024UL;
  char u = v >= 1048576UL ? 'M' : 'K';
  unsigned long t = (unsigned long)(((uint64_t)v * 10 + unit / 2) / unit);   // decimi
  if (t >= 100) snprintf(b, cap, "%lu%c", (t + 5) / 10, u);
  else snprintf(b, cap, "%lu.%lu%c", t / 10, t % 10, u);
}

void printEnt(OutSink& o, const Ent& e, const LsOpt& opt) {
  if (opt.lng) {
    char sz[16];
    fmtSize(sz, sizeof sz, e.size, opt.human);
    o.printf("%c %*s ", e.dir ? 'd' : '-', opt.human ? 6 : 10, sz);
  }
  o.write((const uint8_t*)e.name, e.len);
  if (e.dir) o.print('/');
  o.println();
}

void printList(OutSink& o, EntList& l, const LsOpt& opt) {
  l.sort();
  int shown = 0, skipped = 0;
  for (int i = 0; i < l.count(); i++) {
    if (!opt.all && hidden(l.at(i))) continue;
    if (shown >= LS_SHOW) { skipped++; continue; }
    printEnt(o, l.at(i), opt);
    shown++;
  }
  if (skipped) o.println(trf("... altre %d voci non mostrate", skipped));
  if (l.more()) o.println(tr("... elenco troncato: la cartella ha troppe voci"));
}

// Il percorso e una cartella? Se si la elenca. Ritorna 1 = fatto, 0 = non e una cartella, -1 = errore gia scritto (memoria).
int lsAsDir(OutSink& o, const char* arg, const char* path, const LsOpt& opt, bool header, bool& first) {
  HeapBuf j(FS_JSON_MAX);                                  // l'elenco si scrive qui (heap), non sullo stack
  if (!fsListBuild(path, j)) { o.println(tr("Memoria insufficiente")); return -1; }
  EntList l;
  if (!l.ok()) { o.println(tr("Memoria insufficiente")); return -1; }
  if (!l.parse(const_cast<char*>(j.c_str()))) return 0;    // il JSON e nostro e si puo ripulire sul posto
  if (header) { if (!first) o.println(); o.print(arg); o.println(':'); }
  first = false;
  printList(o, l, opt);
  return 1;
}

// Non e una cartella: cerco la voce nell'elenco della cartella che la contiene (cosi valgono le stesse protezioni).
void lsAsFile(OutSink& o, const char* arg, const char* path, const LsOpt& opt, bool& first) {
  char parent[FS_PATH_MAX + 1];
  const char* slash = strrchr(path, '/');
  const char* base = slash ? slash + 1 : path;
  size_t pl = slash ? (size_t)(slash - path) : 0;
  memcpy(parent, path, pl);
  if (pl == 0) parent[pl++] = '/';                         // "/nome": la cartella e la radice
  parent[pl] = 0;
  HeapBuf j(FS_JSON_MAX);
  if (!fsListBuild(parent, j)) { o.println(tr("Memoria insufficiente")); return; }
  EntList l;
  if (!l.ok()) { o.println(tr("Memoria insufficiente")); return; }
  if (l.parse(const_cast<char*>(j.c_str()))) {
    size_t bl = strlen(base);
    for (int i = 0; i < l.count(); i++) {
      Ent& e = l.at(i);
      if (e.len != bl || memcmp(e.name, base, bl) != 0) continue;
      first = false;
      printEnt(o, e, opt);
      return;
    }
  }
  o.println(trf("ls: %s: Non esiste", arg));
}

// Un percorso gia risolto. arg e il testo digitato (per intestazioni e messaggi).
void lsPath(OutSink& o, const char* arg, const char* path, const LsOpt& opt, bool header, bool& first) {
  if (fsProtected(path)) { o.println(trf("ls: %s: Permesso negato", arg)); return; }
  int r = lsAsDir(o, arg, path, opt, header, first);
  if (r == 0) lsAsFile(o, arg, path, opt, first);
}

// Una parola che comincia con '-' (e non e "-" da solo) e un gruppo di sigle, finche non si incontra "--"
bool isOptWord(const Word& w, bool noMoreOpt) { return !noMoreOpt && w.c_str()[0] == '-' && w.length() > 1 && w != "--"; }

// Legge le sigle di tutta la riga e conta i percorsi. false = errore gia scritto.
bool readOptions(const char* line, OutSink& o, LsOpt& opt, int& np) {
  bool noMoreOpt = false;
  np = 0;
  for (int i = 1; ; i++) {
    Word w = argAt(line, i);
    if (w.empty()) return true;
    if (w.cut()) { o.println(tr("Percorso non valido")); return false; }
    if (!noMoreOpt && w == "--") { noMoreOpt = true; continue; }
    if (!isOptWord(w, noMoreOpt)) {
      if (++np > LS_PATHS) { o.println(trf("ls: troppi percorsi (al massimo %d)", LS_PATHS)); return false; }
      continue;
    }
    for (const char* p = w.c_str() + 1; *p; p++) {
      if (*p == 'a') opt.all = true;
      else if (*p == 'l') opt.lng = true;
      else if (*p == 'h') opt.human = true;
      else if (*p != '1' && *p != 'F') { o.println(trf("ls: opzione non valida '%c'. Uso: ls [-l] [-a] [-h] [-1] [percorso ...]", *p)); return false; }
    }
  }
}

}  // namespace

void shLs(const char* line, OutSink& o, const ShellSess& ss) {
  LsOpt opt;
  int np = 0;
  if (!readOptions(line, o, opt, np)) return;
  bool first = true;
  if (np == 0) { lsPath(o, ".", ss.cwd, opt, false, first); return; }
  bool noMoreOpt = false;
  char path[FS_PATH_MAX + 1];
  for (int i = 1; ; i++) {
    Word w = argAt(line, i);
    if (w.empty()) return;
    if (!noMoreOpt && w == "--") { noMoreOpt = true; continue; }
    if (isOptWord(w, noMoreOpt)) continue;
    if (!shellResolvePath(ss.cwd, w.c_str(), path, sizeof path)) { o.println(trf("ls: %s: Percorso non valido", w.c_str())); continue; }
    lsPath(o, w.c_str(), path, opt, np > 1, first);
  }
}
