// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_files.cpp
// Niente String qui dentro. I percorsi stanno in buffer fissi, dentro un blocco di lavoro preso
// sull'heap (lo stack del web server e piccolo: non ci mettiamo buffer), e i JSON si scrivono in un HeapBuf.
#include "vos_files.h"
#include "vos_i18n.h"
#include "vos_parse.h"
#include "../drivers/vos_drv_fs.h"
#include "../drivers/vos_drv_task.h"
#include "vos_lock.h"

#define MAX_TEXT 32768        // editor della pagina: oltre si scarica soltanto
#define FS_LIST_MAX 200      // voci massime in un elenco cartella (regola API 13): oltre risponde more:true. Molto sopra FS_DIR_MAX: serve per le cartelle
                             // gia piene di versioni precedenti, che non si possono piu riempire ma si possono ancora leggere e svuotare
#define FS_JSON_SOFT 40000   // l'elenco smette di aggiungere voci qui: ne restano 960 byte per l'ultima voce e per la coda
#define FS_DEPTH_MAX 8       // livelli di sottocartelle che si scendono (dimensioni, cancellazione, elenco cartelle)
#define FS_DIRS_LIST_MAX 64  // cartelle massime in fsDirsBuild

// Un percorso pulito (100) + "/" + il nome piu lungo di littlefs (255) + lo zero
#define PATH_BUF (FS_PATH_MAX + 1 + 255 + 1)

namespace {

struct Work {
  char a[PATH_BUF];      // percorso principale: pulito, poi ci si aggiunge "/nome" per scendere nelle cartelle
  char b[PATH_BUF];      // secondo percorso (destinazione di copia e rinomina) o nome temporaneo
  uint8_t io[512];       // copia e lettura a pezzi
};

// Il blocco di lavoro sull'heap, liberato all'uscita. Se manca la memoria il puntatore e nullo.
class WorkPtr {
 public:
  WorkPtr() : w_((Work*)malloc(sizeof(Work))) {}
  ~WorkPtr() { free(w_); }
  WorkPtr(const WorkPtr&) = delete;
  WorkPtr& operator=(const WorkPtr&) = delete;
  Work* operator->() { return w_; }
  explicit operator bool() const { return w_ != nullptr; }
 private:
  Work* w_;
};

// Un solo salvataggio alla volta e la pulizia dei .tmp~ non si pestano i piedi: la pulizia non deve cancellare il temporaneo di un salvataggio in corso.
VosMutex& saveMux() { static VosMutex m = [] { VosMutex v; v.init(); return v; }(); return m; }

bool noMemory(ErrBuf& err) { err = tr("Memoria insufficiente"); return false; }

// Percorso pulito e non protetto in p (PATH_BUF byte)
bool okPath(std::string_view raw, char* p, ErrBuf& err) {
  if (!fsCleanTo(raw, p, PATH_BUF)) { err = tr("Percorso non valido"); return false; }
  if (fsProtected(p)) { err = tr("File di sistema protetto"); return false; }
  return true;
}

// openNextFile() da' il nome corto o il percorso intero, secondo la versione del core: si tiene l'ultima parte
const char* baseName(const char* n) {
  const char* sl = strrchr(n, '/');
  return sl ? sl + 1 : n;
}

// Aggiunge "/nome" al percorso (che e lungo len; per la radice "/" non serve altra barra). Ritorna la nuova lunghezza,
// 0 se non ci sta (non puo capitare con i percorsi che nascono da qui: sono al massimo 100 caratteri).
size_t joinName(char* path, size_t len, const char* name) {
  name = baseName(name);
  size_t k = strlen(name);
  size_t sep = (len == 1 && path[0] == '/') ? 0 : 1;
  if (len + sep + k >= PATH_BUF) return 0;
  if (sep) path[len] = '/';
  memcpy(path + len + sep, name, k);
  path[len + sep + k] = 0;
  return len + sep + k;
}

// Voci (file e sottocartelle) in una cartella, contate fino a limit + 1 (basta per sapere se e piena). 0 se la cartella non c'e.
int dirCount(const char* dir, int limit) {
  VosFile d = drvFsOpen(dir);
  if (!d || !d.isDirectory()) return 0;
  int n = 0;
  for (VosFile c = d.openNextFile(); c; c = d.openNextFile()) { if (++n > limit) break; }
  return n;
}

// Posto per UNA voce nuova in path: la cartella che lo conterra ha meno di FS_DIR_MAX voci. nullptr = c'e posto, altrimenti il testo dell'errore.
// path si modifica un attimo e si ripristina. Perche il tetto: LittleFS costa tempo in proporzione alle voci, e oltre ~30 una sola operazione
// supera i 5 s del Task watchdog. Il numero e dentro il testo: se si cambia FS_DIR_MAX vanno cambiati anche il testo e le sue traduzioni.
const char* roomIn(char* path) {
  static_assert(FS_DIR_MAX == 20, "il testo dell'errore dice 20: aggiornalo (e lang_src.json)");
  char* ls = strrchr(path, '/');
  if (!ls) return nullptr;
  char* cut = (ls == path) ? ls + 1 : ls;                 // per la radice resta "/"
  char keep = *cut;
  *cut = 0;
  int n = dirCount(path, FS_DIR_MAX);
  *cut = keep;
  return n < FS_DIR_MAX ? nullptr : tr("Cartella piena: al massimo 20 voci. Usa una sottocartella");
}

// Stessa cartella contenitore? (rinominare dentro la stessa cartella non aggiunge voci)
bool sameParent(const char* a, const char* b) {
  const char* la = strrchr(a, '/');
  const char* lb = strrchr(b, '/');
  if (!la || !lb) return false;
  size_t na = (size_t)(la - a), nb = (size_t)(lb - b);
  return na == nb && strncmp(a, b, na) == 0;
}

}  // namespace

VosMutex& fsSaveMutex() { return saveMux(); }

bool fsCleanTo(std::string_view in, char* out, size_t cap) {
  if (cap < FS_PATH_MAX + 1) return false;
  in = trimView(in);
  size_t n = 0;
  if (in.empty() || in[0] != '/') out[n++] = '/';          // percorso senza barra iniziale: la si aggiunge
  if (n + in.size() > FS_PATH_MAX) return false;
  memcpy(out + n, in.data(), in.size());
  n += in.size();
  out[n] = 0;
  for (size_t i = 0; i < n; i++) {
    unsigned char c = (unsigned char)out[i];
    if (c < 32 || c >= 127 || c == '\\') return false;
    if (i + 1 < n && ((c == '.' && out[i + 1] == '.') || (c == '/' && out[i + 1] == '/'))) return false;
  }
  if (n > 1 && out[n - 1] == '/') out[n - 1] = 0;
  return true;
}

bool fsProtected(const char* p) {
  return !strncmp(p, "/vesevos.conf", 13) || !strcmp(p, "/audit.log") || !strcmp(p, "/crash.txt");   // configurazione e storico del registro: solo dal firmware
}

// dimensione di una cartella (somma dei file dentro, fino a 8 livelli). path e la cartella: ci si aggiunge "/nome" e al ritorno si toglie.
static size_t dirSize(char* path, size_t len, int depth) {
  if (depth > FS_DEPTH_MAX) return 0;
  VosFile d = drvFsOpen(path);
  if (!d || !d.isDirectory()) return 0;
  size_t t = 0;
  for (VosFile c = d.openNextFile(); c; c = d.openNextFile()) {
    drvTaskPause();
    if (c.isDirectory()) {
      size_t n = joinName(path, len, c.name());
      if (n) { t += dirSize(path, n, depth + 1); path[len] = 0; }
    } else t += c.size();
  }
  return t;
}

static bool addError(HeapBuf& j, const char* text) {
  j.add("{\"ok\":false,\"err\":\"");
  j.addJson(text, strlen(text));
  j.add("\"}");
  return !j.oom() && !j.over();
}

bool fsListBuild(std::string_view raw, HeapBuf& j) {
  WorkPtr w;
  if (!w) return false;
  if (!fsCleanTo(raw, w->a, PATH_BUF)) return addError(j, tr("Percorso non valido"));
  VosFile d = drvFsOpen(w->a);
  if (!d || !d.isDirectory()) return addError(j, tr("Cartella non trovata"));
  size_t plen = strlen(w->a);
  j.add("{\"ok\":true,\"path\":\""); j.addJson(w->a, plen); j.add("\",\"list\":[");
  bool first = true, more = false;
  int cnt = 0;
  for (VosFile f = d.openNextFile(); f; f = d.openNextFile()) {
    drvTaskPause();                                        // idem: elencare una cartella piena non deve affamare il task idle
    const char* name = baseName(f.name());
    size_t full = joinName(w->a, plen, name);              // 0 = percorso troppo lungo: non puo essere un file di sistema
    if (!(full && fsProtected(w->a))) {
      if (cnt >= FS_LIST_MAX || j.length() >= FS_JSON_SOFT) { more = true; w->a[plen] = 0; break; }   // regola API 13: ogni lista ha un limite e dice se e troncata
      cnt++;
      bool dir = f.isDirectory();
      if (!first) j.add(',');
      first = false;
      j.add("{\"n\":\""); j.addJson(name, strlen(name));
      j.add("\",\"d\":"); j.add(dir ? "true" : "false");
      j.add(",\"s\":"); j.addUint((uint32_t)(dir ? (full ? dirSize(w->a, full, 1) : 0) : f.size()));
      j.add('}');
    }
    w->a[plen] = 0;
  }
  j.add("],\"more\":"); j.add(more ? "true" : "false");
  j.add(",\"used\":"); j.addUint((uint32_t)drvFsUsed());
  j.add(",\"total\":"); j.addUint((uint32_t)drvFsTotal());
  j.add(",\"max\":"); j.addUint(MAX_TEXT); j.add('}');
  return !j.oom() && !j.over();
}

const char* fsRoomFor(std::string_view path) {
  WorkPtr w; if (!w) return tr("Memoria insufficiente");
  if (!fsCleanTo(path, w->a, PATH_BUF)) return tr("Percorso non valido");
  if (fsProtected(w->a)) return tr("File di sistema protetto");
  if (drvFsExists(w->a)) return nullptr;                  // esiste gia: si sovrascrive, le voci non aumentano
  return roomIn(w->a);
}

bool fsMkdir(std::string_view raw, ErrBuf& err) {
  WorkPtr w; if (!w) return noMemory(err);
  if (!okPath(raw, w->a, err)) return false;
  if (drvFsExists(w->a)) { err = tr("Esiste gia"); return false; }
  if (const char* full = roomIn(w->a)) { err = full; return false; }
  if (!drvFsMkdir(w->a)) { err = tr("Impossibile creare la cartella"); return false; }
  return true;
}

// Cancella un file o una cartella con tutto il contenuto, un passo alla volta: apre la cartella, prende la PRIMA voce, chiude, e poi
// cancella il file o scende nella sottocartella (quando e vuota la cancella e risale). Cosi non si cancella mai mentre si scorre una
// cartella, e basta un solo percorso in memoria (path, che e la voce da cancellare e si usa come spazio di lavoro).
static bool rmRec(char* path) {
  size_t len = strlen(path);
  const size_t top = len;
  int depth = 0;
  for (int step = 0; step < 20000; step++) {               // tetto di sicurezza: una voce che non sparisce non deve far girare per sempre
    drvTaskPause();                                        // una pausa a ogni passo: in una cartella piena il task idle deve poter girare (Task watchdog)
    VosFile d = drvFsOpen(path);
    if (!d) return false;
    if (!d.isDirectory()) { d.close(); return len == top && drvFsRemove(path); }
    VosFile c = d.openNextFile();
    if (!c) {                                              // cartella vuota
      d.close();
      if (!drvFsRmdir(path)) return false;
      if (len == top) return true;
      len = strrchr(path, '/') - path;                     // risale alla cartella che la contiene
      path[len] = 0;
      depth--;
      continue;
    }
    bool dir = c.isDirectory();
    size_t n = joinName(path, len, c.name());
    c.close(); d.close();
    if (!n) return false;
    if (!dir) {
      if (!drvFsRemove(path)) return false;
      path[len] = 0;
    } else {
      if (depth >= FS_DEPTH_MAX) return false;
      len = n;
      depth++;
    }
  }
  return false;
}

bool fsRemove(std::string_view raw, ErrBuf& err) {
  WorkPtr w; if (!w) return noMemory(err);
  if (!okPath(raw, w->a, err)) return false;
  if (!strcmp(w->a, "/")) { err = tr("Non si puo eliminare la radice"); return false; }
  if (!drvFsExists(w->a)) { err = tr("Non trovato"); return false; }
  if (!rmRec(w->a)) { err = tr("Eliminazione non riuscita"); return false; }
  return true;
}

bool fsRename(std::string_view a, std::string_view b, ErrBuf& err) {
  WorkPtr w; if (!w) return noMemory(err);
  if (!okPath(a, w->a, err) || !okPath(b, w->b, err)) return false;
  if (!drvFsExists(w->a)) { err = tr("Origine non trovata"); return false; }
  if (drvFsExists(w->b)) { err = tr("La destinazione esiste gia"); return false; }
  if (!sameParent(w->a, w->b)) { if (const char* full = roomIn(w->b)) { err = full; return false; } }
  if (!drvFsRename(w->a, w->b)) { err = tr("Rinomina non riuscita"); return false; }
  return true;
}

bool fsCopy(std::string_view a, std::string_view b, ErrBuf& err) {
  WorkPtr w; if (!w) return noMemory(err);
  if (!okPath(a, w->a, err) || !okPath(b, w->b, err)) return false;
  VosFile in = drvFsOpen(w->a, "r");
  if (!in || in.isDirectory()) { err = tr("Origine non e un file"); return false; }
  if (drvFsExists(w->b)) { in.close(); err = tr("La destinazione esiste gia"); return false; }
  if (const char* full = roomIn(w->b)) { in.close(); err = full; return false; }
  if ((size_t)in.size() + 8192 > drvFsTotal() - drvFsUsed()) { in.close(); err = tr("Spazio esaurito"); return false; }
  VosFile out = drvFsOpen(w->b, "w");
  if (!out) { in.close(); err = tr("Impossibile creare la destinazione"); return false; }
  bool ok = true;
  while (in.available()) {
    size_t n = in.read(w->io, sizeof(w->io));
    if (out.write(w->io, n) != n) { ok = false; break; }
  }
  in.close(); out.close();
  if (!ok) { drvFsRemove(w->b); err = tr("Spazio esaurito"); return false; }
  return true;
}

// Scrittura sicura: prima un file temporaneo, poi lo scambio. Se manca la corrente
// a meta, il file vecchio resta intero.
bool fsWriteText(std::string_view raw, std::string_view text, ErrBuf& err, bool mustBeNew) {
  WorkPtr w; if (!w) return noMemory(err);
  char* p = w->a;
  if (!okPath(raw, p, err)) return false;
  size_t plen = strlen(p);
  if (plen == 1) { err = tr("Nome file mancante"); return false; }     // "/"
  if (plen >= 5 && !strcmp(p + plen - 5, ".tmp~")) { err = tr("Nome non valido"); return false; }
  if (text.size() > MAX_TEXT) { err = tr("Testo troppo lungo (max 32 KB)"); return false; }
  bool exists = drvFsExists(p);
  if (exists) { VosFile d = drvFsOpen(p); bool dir = d && d.isDirectory(); d.close(); if (dir) { err = tr("Esiste una cartella con questo nome"); return false; } }
  if (mustBeNew && exists) { err = tr("Esiste gia"); return false; }
  size_t freeB = drvFsTotal() - drvFsUsed();
  if (text.size() + 8192 > freeB) { err = tr("Spazio esaurito"); return false; }
  char* ls = strrchr(p, '/');
  if (ls > p) {                                                         // la cartella che la contiene (se non e la radice) deve esistere
    *ls = 0;
    bool dirOk = drvFsExists(p);
    *ls = '/';
    if (!dirOk) { err = tr("Cartella non trovata"); return false; }
  }
  if (!exists) { if (const char* full = roomIn(p)) { err = full; return false; } }   // un file nuovo aggiunge una voce; sovrascrivere no
  snprintf(w->b, PATH_BUF, "%s.tmp~", p);
  VosLock lk(saveMux());                                                // fino all'uscita: scrittura del temporaneo e scambio
  VosFile f = drvFsOpen(w->b, "w");
  if (!f) { err = tr("Impossibile scrivere"); return false; }
  size_t wr = f.write((const uint8_t*)text.data(), text.size());
  f.close();
  if (wr != text.size()) { drvFsRemove(w->b); err = tr("Spazio esaurito"); return false; }
  // rename() su LittleFS sostituisce il file che c'e in un solo passo (un'unica scrittura di directory):
  // non si cancella prima il vecchio, cosi non c'e un momento in cui il file manca.
  // Se la rinomina fallisce (es. file aperto) il vecchio resta intero e si toglie il temporaneo.
  if (!drvFsRename(w->b, p)) { drvFsRemove(w->b); err = tr("Impossibile scrivere"); return false; }
  return true;
}

// Legge un file di testo (UTF-8 com'e, accenti compresi). Rifiuta file binari e troppo grandi.
bool fsReadText(std::string_view raw, HeapBuf& out, ErrBuf& err) {
  WorkPtr w; if (!w) return noMemory(err);
  if (!okPath(raw, w->a, err)) return false;
  VosFile f = drvFsOpen(w->a, "r");
  if (!f || f.isDirectory()) { err = tr("File non trovato"); return false; }
  if (f.size() > MAX_TEXT) { f.close(); err = tr("File troppo grande per l'editor (max 32 KB): scaricalo"); return false; }
  while (f.available()) {
    size_t n = f.read(w->io, sizeof(w->io));
    if (n == 0) break;
    if (memchr(w->io, 0, n)) { f.close(); out.clear(); err = tr("File binario: non si puo modificare, scaricalo"); return false; }
    if (!out.add((const char*)w->io, n)) { f.close(); out.clear(); err = tr("Memoria insufficiente"); return false; }
  }
  f.close();
  return true;
}

// Elenco di tutte le cartelle (per "Sposta"): ["/","/lang",...]. path e la cartella che si scorre (si usa come spazio di lavoro).
static void dirsRec(char* path, size_t len, HeapBuf& j, int depth, int& n) {
  if (depth > FS_DEPTH_MAX || n >= FS_DIRS_LIST_MAX) return;
  VosFile d = drvFsOpen(path);
  if (!d || !d.isDirectory()) return;
  for (VosFile c = d.openNextFile(); c && n < FS_DIRS_LIST_MAX; c = d.openNextFile()) {
    if (!c.isDirectory()) continue;
    size_t full = joinName(path, len, c.name());
    if (!full) continue;
    j.add(",\""); j.addJson(path, full); j.add('"'); n++;
    dirsRec(path, full, j, depth + 1, n);
    path[len] = 0;
  }
}

bool fsDirsBuild(HeapBuf& j) {
  WorkPtr w;
  if (!w) return false;
  j.add("[\"/\"");
  int n = 0;
  strcpy(w->a, "/");
  dirsRec(w->a, 1, j, 0, n);
  j.add(']');
  return !j.oom() && !j.over();
}

size_t fsTextMax() { return MAX_TEXT; }

// ---- pulizia dei temporanei .tmp~ ----
namespace {
struct TmpWork {
  char def[PATH_BUF];                     // percorso del definitivo
  char tmp[PATH_BUF];                     // percorso del temporaneo
  char cand[FS_TMP_CAND_MAX][FS_PATH_MAX + 1];   // nomi (senza ".tmp~") raccolti mentre si scorre la cartella
};
const char* const TMP_DIRS[] = {"/", "/scripts", "/apps", "/lang"};   // dove salva di solito la pagina. Niente scansione di tutto il file system
}  // namespace

void fsTmpClean(const char* dir, FsTmpStat& st) {
  TmpWork* w = (TmpWork*)malloc(sizeof(TmpWork));
  if (!w) return;
  size_t dlen = strlen(dir);
  int seen = 0, nc = 0;
  {                                       // 1) si scorre la cartella senza cambiare nulla, poi la si chiude
    VosFile d = drvFsOpen(dir);
    if (d && d.isDirectory()) {
      for (VosFile c = d.openNextFile(); c; c = d.openNextFile()) {
        drvTaskPause();
        if (++seen > FS_TMP_SCAN_MAX) { st.more = true; break; }
        if (c.isDirectory()) continue;
        const char* nm = baseName(c.name());
        size_t k = strlen(nm);
        if (k <= 5 || strcmp(nm + k - 5, ".tmp~") != 0) continue;
        if (nc >= FS_TMP_CAND_MAX || k - 5 > FS_PATH_MAX) { st.more = true; continue; }
        memcpy(w->cand[nc], nm, k - 5);
        w->cand[nc][k - 5] = 0;
        nc++;
      }
    }
  }
  st.scanned += (uint16_t)(seen > FS_TMP_SCAN_MAX ? FS_TMP_SCAN_MAX : seen);
  for (int i = 0; i < nc; i++) {          // 2) si decide e si cancella, una voce alla volta
    drvTaskPause();
    memcpy(w->def, dir, dlen + 1);
    size_t len = joinName(w->def, dlen, w->cand[i]);
    if (!len || len + 5 >= PATH_BUF) { st.more = true; continue; }
    snprintf(w->tmp, PATH_BUF, "%s.tmp~", w->def);
    if (fsProtected(w->def) || fsProtected(w->tmp)) continue;
    VosLock lk(saveMux());                // un salvataggio in corso ha il suo temporaneo: aspetta che finisca, poi ricontrolla
    VosFile f = drvFsOpen(w->def);
    bool defFile = f && !f.isDirectory();
    f.close();
    if (!defFile) {                       // niente definitivo: il temporaneo puo essere l'unica copia
      if (!st.orphan++) snprintf(st.firstOrphan, sizeof(st.firstOrphan), "%s", w->def);
      continue;
    }
    VosFile t = drvFsOpen(w->tmp);
    bool tmpFile = t && !t.isDirectory();
    uint32_t tmpBytes = tmpFile ? (uint32_t)t.size() : 0;
    t.close();
    if (!tmpFile) continue;               // sparito nel frattempo (salvataggio finito)
    if (st.dryRun) { st.removed++; st.bytes += tmpBytes; continue; }   // prova a secco: si conta e basta
    if (drvFsRemove(w->tmp)) { st.removed++; st.bytes += tmpBytes; } else st.failed++;
  }
  free(w);
}

void fsTmpCleanKnown(FsTmpStat& st) {
  for (const char* d : TMP_DIRS) { fsTmpClean(d, st); drvTaskPause(); }
}
