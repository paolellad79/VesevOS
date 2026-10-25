// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_shell_tar.cpp
// Comando tar in stile shell classica: tar -c|-t|-x [-v] [-f archivio] [-C cartella] [--overwrite] [file...]  (anche unito: tar -xf a.tar, tar xvf a.tar)
// Solo il CONTENITORE ustar, senza compressione (per .tar.gz: gunzip e poi tar). File e cartelle; niente collegamenti ne' file speciali ne' formati
// pax/estesi. L'archivio deve stare su LittleFS (-f e obbligatorio: la shell non ha uno standard input/output per i dati).
// Lettura: PRIMA si controlla tutto l'archivio (somme di controllo, nomi, tipi, dimensioni), POI si estrae, esattamente come unzip (vos_shell_zip.cpp):
// stesse protezioni contro zip slip (nomi con '/' iniziale, '..', '\'), stessi file protetti, stessi tetti di voci e di dimensione. tar non comprime, quindi
// il rapporto di espansione non c'e: bastano i tetti. Di fabbrica NON sovrascrive i file che ci sono (il tar classico si: qui serve --overwrite).
// Creazione: vos_shell_tarc.cpp.
#include "vos_shell_tar.h"

using namespace shx;

namespace {

struct TarEntry {
  char   name[ARCH_NAME_MAX + 1];           // nome sicuro, relativo ("" = voce da ignorare)
  bool   isDir;
  size_t size;
  size_t dataOfs;
};

// Memoria di lavoro sull'heap (lo stack del server web e piccolo)
struct Work {
  uint8_t  blk[TAR_BLOCK];
  TarEntry e;
  char     target[FS_PATH_MAX + 1];
  char     parent[FS_PATH_MAX + 1];
  ArchIo   io;
};

// Numero ottale in un campo di n byte (spazi e zeri iniziali, chiuso da zero o spazio). false se non e un numero ottale.
bool parseOctal(const uint8_t* p, size_t n, uint64_t& v) {
  size_t i = 0;
  while (i < n && p[i] == ' ') i++;
  if (i == n || p[i] < '0' || p[i] > '7') return false;
  v = 0;
  for (; i < n && p[i] >= '0' && p[i] <= '7'; i++) v = (v << 3) | (uint64_t)(p[i] - '0');
  return true;
}

// Legge le intestazioni una dopo l'altra. next(): 1 = una voce (e valida; il file e all'inizio dei dati), 0 = fine, -1 = errore (why).
struct TarIter {
  VosFile&    f;
  size_t      fsize;
  Work&       w;
  size_t      pos = 0;
  const char* why = nullptr;

  int next() {
    if (pos >= fsize) return 0;
    if (pos + TAR_BLOCK > fsize || !f.seek(pos) || f.read(w.blk, TAR_BLOCK) != TAR_BLOCK) return fail(tr("Archivio troncato o danneggiato"));
    uint8_t any = 0;
    for (size_t i = 0; i < TAR_BLOCK; i++) any |= w.blk[i];
    if (!any) return 0;                                                          // blocco di zeri: fine dell'archivio
    uint64_t stored = 0, size = 0;
    uint32_t sum = 0;
    for (size_t i = 0; i < TAR_BLOCK; i++) sum += (i >= 148 && i < 156) ? ' ' : w.blk[i];   // la somma conta il campo stesso come spazi
    if (!parseOctal(w.blk + 148, 8, stored) || stored != sum) return fail(tr("Non e un archivio tar valido"));
    uint8_t type = w.blk[156];
    if (type == 'x' || type == 'g' || type == 'L' || type == 'K') return fail(tr("Formato tar non supportato (pax o nomi lunghi estesi)"));
    if (type != '0' && type != 0 && type != '5') return fail(tr("Voce non supportata (collegamento o file speciale)"));
    if ((w.blk[124] & 0x80) || !parseOctal(w.blk + 124, 12, size) || size > 0x7FFFFFFFu) return fail(tr("Voce troppo grande o non valida"));
    char raw[ARCH_NAME_MAX + 2];                                                 // prefisso ustar + '/' + nome, al massimo 100 caratteri
    size_t pl = strnlen((const char*)w.blk + 345, 155), nl = strnlen((const char*)w.blk, 100);
    if (nl == 0 || pl + (pl ? 1 : 0) + nl > ARCH_NAME_MAX) return fail(tr("Nome non sicuro: archivio rifiutato"));
    size_t n = 0;
    if (pl) { memcpy(raw, w.blk + 345, pl); n = pl; raw[n++] = '/'; }
    memcpy(raw + n, w.blk, nl);
    n += nl;
    bool dirName = false;
    if (!archSafeName(raw, n, w.e.name, sizeof w.e.name, &dirName)) return fail(tr("Nome non sicuro: archivio rifiutato"));   // zip slip
    w.e.isDir = (type == '5') || dirName;
    w.e.size = (size_t)size;
    w.e.dataOfs = pos + TAR_BLOCK;
    if (w.e.isDir && size) return fail(tr("Voce troppo grande o non valida"));
    size_t padded = ((size_t)size + TAR_BLOCK - 1) / TAR_BLOCK * TAR_BLOCK;
    if (w.e.dataOfs + size > fsize) return fail(tr("Archivio troncato o danneggiato"));
    pos = w.e.dataOfs + padded;
    return 1;
  }
 private:
  int fail(const char* msg) { why = msg; return -1; }
};

// Passa tutte le voci prima di scrivere qualcosa. dest = destinazione (nullptr = solo elenco); print = scrive l'elenco (tar -t).
// mask: bit i = la voce i va estratta. Falso (messaggio scritto) se qualcosa non va.
bool checkAll(ArchCtx& c, TarIter& it, const TarOpts& op, const char* line, const char* dest, bool print, uint64_t& mask, unsigned& nSel) {
  size_t budget = ArchCtx::budget(), total = 0;
  unsigned n = 0;
  mask = 0;
  nSel = 0;
  int r;
  while ((r = it.next()) == 1) {
    const TarEntry& e = it.w.e;
    if (++n > ARCH_MAX_ENTRIES) { c.fail(op.archive.c_str(), tr("Troppe voci (massimo 64)")); return false; }
    if (e.size > ARCH_MAX_ENTRY) { c.fail(e.name, trf("Voce troppo grande (massimo %u KB)", (unsigned)(ARCH_MAX_ENTRY / 1024))); return false; }
    total += e.size;
    if (dest && total > budget) { c.fail(op.archive.c_str(), trf("Troppo grande dopo l'espansione (massimo %u KB, o manca spazio)", (unsigned)(budget / 1024))); return false; }
    if (dest && e.name[0]) {
      if (!archTarget(dest, e.name, it.w.target)) { c.fail(e.name, tr("Nome non sicuro: archivio rifiutato")); return false; }
      if (fsProtected(it.w.target)) { c.fail(e.name, tr("File di sistema protetto")); return false; }
    }
    if (e.name[0] && archSelected(line, op.firstOperand, e.name)) {
      mask |= (uint64_t)1 << (n - 1);
      nSel++;
      if (print) {
        if (op.verbose) c.out().printf("%8lu ", (unsigned long)e.size);
        c.out().printf("%s%s\r\n", e.name, e.isDir ? "/" : "");
      }
    }
    DrvDefErr g = c.guard().tick(TAR_BLOCK);
    if (g != DDF_OK) { c.failDef(op.archive.c_str(), g, nullptr); return false; }
  }
  if (r < 0) { c.fail(op.archive.c_str(), it.why); return false; }
  return true;
}

// Estrae la voce corrente (it.w.e, il file e all'inizio dei dati). X_OK / X_SKIP (esiste gia) / X_FAIL.
enum Result { X_OK, X_SKIP, X_FAIL };

Result extractOne(ArchCtx& c, TarIter& it, const char* dest, const TarOpts& op) {
  Work& w = it.w;
  const TarEntry& e = w.e;
  ErrBuf err;
  if (!archTarget(dest, e.name, w.target)) { c.fail(e.name, tr("Nome non sicuro: archivio rifiutato")); return X_FAIL; }
  if (e.isDir) {
    if (!fsMkdirs(w.target, err)) { c.fail(e.name, err.c_str()); return X_FAIL; }
    if (op.verbose) c.out().printf("%s/\r\n", e.name);
    return X_OK;
  }
  snprintf(w.parent, sizeof w.parent, "%s", w.target);
  *strrchr(w.parent, '/') = 0;
  if (!fsMkdirs(w.parent, err)) { c.fail(e.name, err.c_str()); return X_FAIL; }
  if (drvFsExists(w.target)) {
    VosFile old = drvFsOpen(w.target);
    if (old && old.isDirectory()) { c.fail(e.name, tr("Esiste una cartella con questo nome")); return X_FAIL; }
    old.close();
    if (!op.overwrite) { c.fail(e.name, tr("Esiste gia, saltato (usa --overwrite)")); return X_SKIP; }
  }
  ArchFileOut out;
  if (!out.begin(w.target, op.overwrite, e.size, e.size, err)) { c.fail(e.name, err.c_str()); return X_FAIL; }
  size_t rem = e.size;
  DrvDefErr de = it.f.seek(e.dataOfs) ? DDF_OK : DDF_BAD_DATA;
  while (de == DDF_OK && rem) {
    size_t n = it.f.read(w.io.b, rem < sizeof w.io.b ? rem : sizeof w.io.b);
    if (n == 0) { de = DDF_BAD_DATA; break; }
    rem -= n;
    de = out.put(w.io.b, n) ? c.guard().tick(n) : DDF_SINK;
  }
  if (de != DDF_OK) { out.abort(); c.failDef(e.name, de, out.why()); return X_FAIL; }
  if (!out.finish(err)) { c.fail(e.name, err.c_str()); return X_FAIL; }
  if (op.verbose) c.out().printf("%s\r\n", e.name);
  return X_OK;
}

// ---------- opzioni ----------

bool onlyLetters(const Word& w, const char* set) {
  for (const char* p = w.c_str(); *p; p++)
    if (!strchr(set, *p)) return false;
  return !w.empty();
}

bool parseArgs(const char* line, OutSink& o, TarOpts& op) {
  for (int i = 1; i < 40; i++) {
    Word w = argAt(line, i);
    if (w.empty()) break;
    if (w.cut()) { o.println(tr("Percorso non valido")); return false; }
    if (w == "--overwrite") { op.overwrite = true; continue; }
    if (w.startsWith("--")) { o.println(trf("tar: opzione non valida -- '%s'", w.c_str())); return false; }
    bool dash = isShortOpt(w);
    if (!dash && !(i == 1 && onlyLetters(w, "ctxvfC"))) { op.firstOperand = i; break; }   // "tar xf a.tar": la prima parola puo essere senza '-'
    char wants[3] = "";                                                                // f e C vogliono un valore: la parola dopo, nell'ordine
    size_t nw = 0;
    for (const char* p = w.c_str() + (dash ? 1 : 0); *p; p++) {
      if (strchr("ctx", *p)) { if (op.mode && op.mode != *p) { o.println(tr("tar: serve una sola tra -c, -t e -x")); return false; } op.mode = *p; }
      else if (*p == 'v') op.verbose = true;
      else if (*p == 'f' || *p == 'C') { if (nw < 2) wants[nw++] = *p; }
      else { o.println(trf("tar: opzione non valida -- '%c'", *p)); cmdUsage(o, "tar"); return false; }
    }
    for (size_t k = 0; k < nw; k++) {
      Word v = argAt(line, ++i);
      if (v.empty()) { o.println(trf("tar: l'opzione -%c vuole un valore", wants[k])); return false; }
      if (wants[k] == 'f') op.archive = v; else op.dir = v;
    }
  }
  if (!op.mode) { cmdUsage(o, "tar"); return false; }
  if (op.archive.empty()) { o.println(tr("tar: serve -f <archivio> (la shell non ha uno standard input per i dati)")); return false; }
  return true;
}

}  // namespace

void shTar(const char* line, OutSink& o, const ShellSess& ss, uint32_t sess) {
  TarOpts op;
  if (!parseArgs(line, o, op)) return;
  ArchCtx c("tar", o, sess);
  char path[FS_PATH_MAX + 1], base[FS_PATH_MAX + 1];
  if (!shellResolvePath(ss.cwd, op.archive.c_str(), path, sizeof path)) { c.fail(op.archive.c_str(), tr("Percorso non valido")); return; }
  if (op.dir.empty()) snprintf(base, sizeof base, "%s", ss.cwd);
  else if (!shellResolvePath(ss.cwd, op.dir.c_str(), base, sizeof base)) { c.fail(op.dir.c_str(), tr("Percorso non valido")); return; }
  if (fsProtected(path) || fsProtected(base)) { c.fail(op.archive.c_str(), tr("File di sistema protetto")); return; }
  if (op.mode == 'x' && strcmp(base, "/") == 0) { c.fail("", tr("Non si estrae nella radice: scegli una cartella con -C")); return; }
  if (!c.begin()) return;

  if (op.mode == 'c') { tarCreate(c, op, line, path, base); return; }

  VosFile f = drvFsOpen(path, "r");
  if (!f) { c.fail(op.archive.c_str(), tr("File non trovato")); return; }
  if (f.isDirectory()) { c.fail(op.archive.c_str(), tr("E una cartella, ignorata")); return; }
  size_t size = f.size();
  if (size > c.lim().maxIn) { c.tooBig(op.archive.c_str(), c.lim().maxIn); return; }
  ArchHeap<Work> w;
  if (!w) { c.fail("", tr("Memoria insufficiente")); return; }

  TarIter it = {f, size, *w};
  uint64_t mask = 0;
  unsigned nSel = 0;
  bool list = (op.mode == 't');
  if (!checkAll(c, it, op, line, list ? nullptr : base, list, mask, nSel) || list) return;
  if (nSel == 0) { c.fail(op.archive.c_str(), tr("Nessuna voce corrisponde")); return; }

  ErrBuf err;
  if (!fsMkdirs(base, err)) { c.fail(base, err.c_str()); return; }
  it.pos = 0;
  unsigned done = 0, skipped = 0;
  for (unsigned i = 0; !c.stopped() && it.next() == 1; i++) {
    if (!((mask >> i) & 1)) continue;
    Result r = extractOne(c, it, base, op);
    if (r == X_OK) done++;
    else if (r == X_SKIP) skipped++;
    else return;                                                         // un errore ferma tutto: i file gia estratti restano interi
  }
  o.println(trf("%s: %u estratti, %u saltati", "tar", done, skipped));
}
