// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_shell_tarc.cpp
// tar -c: crea un archivio ustar (solo contenitore, nessuna compressione) da file e cartelle di LittleFS.
// FORMATO scritto: un'intestazione da 512 byte per voce (nome fino a 100 caratteri, senza il campo "prefisso"), i dati riempiti a 512, poi due blocchi
// di zeri. Permessi fissi 0644 (file) e 0755 (cartelle), uid e gid 0, data (mtime) 0: niente nomi utente ne' ore dentro l'archivio.
// I nomi sono relativi a -C (o alla cartella corrente); un percorso fuori da li perde la '/' iniziale, come nel tar classico.
// LIMITI: al massimo ARCH_MAX_ENTRIES voci, ARCH_MAX_ENTRY byte per file, e in tutto il tetto di ingresso della sessione (128 KB dalla pagina web);
// TAR_DEPTH_MAX livelli di cartelle. I file protetti si saltano con un avviso. L'archivio stesso (e il suo temporaneo) non entra mai dentro se stesso.
// Si scrive in "archivio.tmp~" e si scambia alla fine (FsAtomicWriter): un errore non lascia un archivio a meta.
#include "vos_shell_tar.h"

using namespace shx;

namespace {

struct TWork {
  uint8_t blk[TAR_BLOCK];
  ArchIo  io;
};

struct Build {
  Build(ArchCtx& ctx, TWork& work, const char* archivePath) : c(ctx), w(work), archive(archivePath) {}
  ArchCtx&     c;
  ArchFileOut  out;
  TWork&       w;
  const char*  archive;                            // percorso risolto dell'archivio (da non archiviare)
  unsigned     entries = 0;
  size_t       inBytes = 0;
  bool         fatal = false;                      // un errore che ferma tutto
};

// ---------- intestazione ustar ----------

// Numero ottale in un campo di width byte (width - 1 cifre con zeri davanti, poi uno zero)
void putOctal(uint8_t* dst, size_t width, uint64_t v) {
  for (size_t i = width - 1; i-- > 0;) { dst[i] = (uint8_t)('0' + (v & 7)); v >>= 3; }
  dst[width - 1] = 0;
}

void makeHeader(uint8_t* h, const char* name, size_t size, bool dir) {
  memset(h, 0, TAR_BLOCK);
  memcpy(h, name, strlen(name));                     // il chiamante ha controllato: al massimo 100 caratteri
  putOctal(h + 100, 8, dir ? 0755 : 0644);
  putOctal(h + 108, 8, 0);                           // uid
  putOctal(h + 116, 8, 0);                           // gid
  putOctal(h + 124, 12, size);
  putOctal(h + 136, 12, 0);                          // mtime: 0, nessuna data
  memset(h + 148, ' ', 8);                           // per la somma il campo conta come spazi
  h[156] = dir ? '5' : '0';
  memcpy(h + 257, "ustar", 6);                       // "ustar" e il suo zero
  memcpy(h + 263, "00", 2);
  putOctal(h + 329, 8, 0);                           // devmajor
  putOctal(h + 337, 8, 0);                           // devminor
  uint32_t sum = 0;
  for (size_t i = 0; i < TAR_BLOCK; i++) sum += h[i];
  putOctal(h + 148, 7, sum);                         // sei cifre e uno zero, poi lo spazio gia messo
}

// ---------- scrittura ----------

// Un errore di scrittura nell'archivio: lo dice e ferma tutto
bool putFail(Build& b, const char* name, DrvDefErr e) {
  b.c.failDef(name, e, b.out.why());
  b.fatal = true;
  return false;
}

bool putBlock(Build& b, const char* name, const uint8_t* data, size_t n) {
  if (!b.out.put(data, n)) return putFail(b, name, DDF_SINK);
  DrvDefErr g = b.c.guard().tick(n);
  return g == DDF_OK ? true : putFail(b, name, g);
}

bool header(Build& b, const char* rel, size_t size, bool dir) {
  char name[ARCH_NAME_MAX + 2];
  snprintf(name, sizeof name, "%s%s", rel, dir ? "/" : "");
  if (strlen(name) > ARCH_NAME_MAX) { b.c.fail(rel, tr("Nome troppo lungo per tar (massimo 100 caratteri)")); b.fatal = true; return false; }
  makeHeader(b.w.blk, name, size, dir);
  return putBlock(b, rel, b.w.blk, TAR_BLOCK);
}

bool addFile(Build& b, VosFile& f, const char* rel) {
  size_t size = f.size();
  if (size > ARCH_MAX_ENTRY) { b.c.tooBig(rel, ARCH_MAX_ENTRY); b.fatal = true; return false; }
  b.inBytes += size;
  if (b.inBytes > b.c.lim().maxIn) { b.c.tooBig(rel, b.c.lim().maxIn); b.fatal = true; return false; }
  if (!header(b, rel, size, false)) return false;
  size_t left = size;
  while (left) {
    size_t n = f.read(b.w.io.b, left < sizeof b.w.io.b ? left : sizeof b.w.io.b);
    if (n == 0) { b.c.fail(rel, tr("Il file e cambiato durante la lettura")); b.fatal = true; return false; }
    if (!putBlock(b, rel, b.w.io.b, n)) return false;
    left -= n;
  }
  size_t pad = (TAR_BLOCK - size % TAR_BLOCK) % TAR_BLOCK;
  if (pad) { memset(b.w.blk, 0, pad); return putBlock(b, rel, b.w.blk, pad); }
  return true;
}

// ---------- cammino nelle cartelle ----------

bool endsTmp(const char* s) {
  size_t n = strlen(s);
  return n >= 5 && strcmp(s + n - 5, ".tmp~") == 0;
}

// Aggiunge abs (file o cartella) con il nome rel. rel vuoto = la cartella stessa non ha intestazione (se ne aggiungono solo i figli).
bool addTree(Build& b, const char* abs, const char* rel, unsigned depth) {
  if (fsProtected(abs)) { b.c.fail(rel[0] ? rel : abs, tr("File di sistema protetto, saltato")); return true; }
  if (strcmp(abs, b.archive) == 0) { b.c.fail(rel, tr("E l'archivio stesso, saltato")); return true; }
  if (endsTmp(abs)) return true;
  VosFile f = drvFsOpen(abs, "r");
  if (!f) { b.c.fail(rel[0] ? rel : abs, tr("File non trovato")); b.fatal = true; return false; }
  if (++b.entries > ARCH_MAX_ENTRIES) { b.c.fail("", tr("Troppe voci (massimo 64)")); b.fatal = true; return false; }
  if (!f.isDirectory()) {
    if (!rel[0]) { b.c.fail(abs, tr("Percorso non valido")); b.fatal = true; return false; }   // -C che indica un file
    return addFile(b, f, rel);
  }

  if (depth > TAR_DEPTH_MAX) { b.c.fail(rel, tr("Cartelle troppo profonde")); b.fatal = true; return false; }
  if (rel[0] && !header(b, rel, 0, true)) return false;
  ArchHeap<ArchKids> kids;                           // i nomi dei figli: poi la cartella si chiude, cosi non restano aperte piu cartelle insieme
  if (!kids) { b.c.fail(rel, tr("Memoria insufficiente")); b.fatal = true; return false; }
  if (!archReadKids(f, *kids)) { b.c.fail(rel, tr("Cartella con troppe voci")); b.fatal = true; return false; }
  f.close();
  for (unsigned i = 0; i < kids->count && !b.fatal; i++) {
    char childAbs[FS_PATH_MAX + 1], childRel[ARCH_NAME_MAX + 1];
    int na = snprintf(childAbs, sizeof childAbs, "%s%s%s", abs, strcmp(abs, "/") ? "/" : "", kids->name[i]);
    int nr = snprintf(childRel, sizeof childRel, "%s%s%s", rel, rel[0] ? "/" : "", kids->name[i]);
    if (na < 0 || na > (int)FS_PATH_MAX || nr < 0 || nr > (int)ARCH_NAME_MAX) { b.c.fail(kids->name[i], tr("Nome troppo lungo per tar (massimo 100 caratteri)")); b.fatal = true; return false; }
    if (!addTree(b, childAbs, childRel, depth + 1)) return false;
  }
  return true;
}

}  // namespace

void shx::tarCreate(ArchCtx& c, const TarOpts& op, const char* line, const char* archive, const char* base) {
  if (op.firstOperand == 0) { c.fail("", tr("Rifiuto di creare un archivio vuoto")); return; }
  ArchHeap<TWork> w;
  if (!w) { c.fail("", tr("Memoria insufficiente")); return; }
  Build b(c, *w, archive);
  ErrBuf err;
  if (!op.overwrite && drvFsExists(archive)) { c.fail(op.archive.c_str(), tr("Esiste gia, non sovrascritto (usa --overwrite)")); return; }
  if (!b.out.begin(archive, true, (size_t)-1, c.lim().maxIn + 80 * 1024, err)) { c.fail(op.archive.c_str(), err.c_str()); return; }

  bool stripped = false;
  for (int i = op.firstOperand; i < 40 && !b.fatal; i++) {
    Word wd = argAt(line, i);
    if (wd.empty()) break;
    char abs[FS_PATH_MAX + 1];
    if (wd.cut() || !shellResolvePath(base, wd.c_str(), abs, sizeof abs)) { c.fail(wd.c_str(), tr("Percorso non valido")); b.out.abort(); return; }
    addTree(b, abs, archRelName(base, abs, stripped), 0);
  }
  if (b.fatal) { b.out.abort(); return; }
  if (stripped) c.out().println(trf("%s: %s", "tar", tr("Tolgo la '/' iniziale dai nomi")));
  if (b.entries == 0) { b.out.abort(); c.fail("", tr("Rifiuto di creare un archivio vuoto")); return; }
  memset(w->blk, 0, TAR_BLOCK);
  if (!putBlock(b, "", w->blk, TAR_BLOCK) || !putBlock(b, "", w->blk, TAR_BLOCK)) { b.out.abort(); return; }   // fine dell'archivio: due blocchi di zeri
  size_t total = b.out.bytes();
  if (!b.out.finish(err)) { c.fail(op.archive.c_str(), err.c_str()); return; }
  c.out().println(trf("%s: creato %s (%u voci, %u byte)", "tar", op.archive.c_str(), b.entries, (unsigned)total));
}
