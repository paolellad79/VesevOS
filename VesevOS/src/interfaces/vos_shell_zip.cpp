// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_shell_zip.cpp
// Comando unzip in stile shell classica: unzip [-l] [-o] [-d cartella] archivio.zip [file...]. SOLO lettura ed estrazione (la creazione e il comando zip, in vos_shell_zipc.cpp).
// Si leggono lo "end of central directory" e la "central directory" in fondo al file (con seek): hanno dimensioni, CRC e metodo veri.
// Metodi 0 (store) e 8 (deflate), CRC32 controllato per ogni file. NON supportati: ZIP64, cifratura, archivi a piu volumi, collegamenti simbolici,
// commento finale oltre 1 KB. I nomi sono solo ASCII (regola del progetto).
// PRIMA di scrivere qualcosa si controlla TUTTO l'archivio (nomi, metodi, dimensioni, rapporti): se una sola voce e pericolosa
// (zip slip, file protetto, zip bomb) l'archivio e rifiutato per intero. Le protezioni sono in vos_shell_arch.h.
// Si estrae nella cartella corrente o in quella data con -d, MAI nella radice "/". Senza -o un file che c'e gia non si tocca (si salta).
// I filtri "file..." sono nomi esatti (o una cartella: vale per tutto cio che c'e dentro); niente caratteri jolly.
#include "vos_shell_arch.h"

using namespace shx;

namespace {

const uint32_t kSigEocd = 0x06054b50, kSigCen = 0x02014b50, kSigLoc = 0x04034b50;
const size_t   kEocdLen = 22, kCenLen = 46, kLocLen = 30;
const size_t   kEocdSearch = 1024 + kEocdLen;        // si cerca la fine nell'ultimo KB (piu il commento ammesso)

uint16_t le16(const uint8_t* p) { return (uint16_t)(p[0] | (p[1] << 8)); }
uint32_t le32(const uint8_t* p) { return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24); }

struct Entry {
  char     name[ARCH_NAME_MAX + 1];    // nome sicuro, relativo, senza '/' iniziale e finale ("" = voce da ignorare)
  bool     isDir;
  uint16_t method;
  uint32_t crc, csize, usize, localOfs;
};

// Memoria di lavoro dell'estrazione, sull'heap (lo stack del server web e piccolo)
struct Work {
  Entry   e;
  char    raw[ARCH_NAME_MAX + 1];
  char    target[FS_PATH_MAX + 1];
  char    parent[FS_PATH_MAX + 1];
  ArchIo  io;
};

struct Zip {
  VosFile  f;
  size_t   size = 0;
  uint32_t cdOfs = 0;
  unsigned count = 0;
};

struct Opts {
  bool list = false, overwrite = false;
  Word dir;                                          // -d (vuoto = cartella corrente)
  Word archive;
  int  firstFilter = 0;                              // indice della prima parola dopo l'archivio (0 = nessun filtro)
};

// ---------- lettura dell'archivio ----------

// Trova la fine dell'archivio e legge dove sta l'elenco. Ritorna nullptr se va bene, altrimenti il motivo (sempre un testo fisso di tr()).
const char* openZip(Zip& z, Work& w) {
  if (z.size < kEocdLen) return tr("Non e un archivio zip valido");
  size_t win = z.size < kEocdSearch ? z.size : kEocdSearch;
  if (!z.f.seek(z.size - win) || z.f.read(w.io.b, win) != win) return tr("Non e un archivio zip valido");
  for (size_t i = win - kEocdLen + 1; i-- > 0;) {
    const uint8_t* p = w.io.b + i;
    if (le32(p) != kSigEocd || i + kEocdLen + le16(p + 20) != win) continue;       // anche il commento deve arrivare esatto a fine file
    uint16_t entries = le16(p + 10);
    uint32_t cdSize = le32(p + 12), cdOfs = le32(p + 16);
    if (le16(p + 4) || le16(p + 6) || le16(p + 8) != entries) return tr("ZIP64, cifratura o piu volumi: non supportati");
    if (entries == 0xFFFF || cdSize == 0xFFFFFFFFu || cdOfs == 0xFFFFFFFFu) return tr("ZIP64, cifratura o piu volumi: non supportati");
    if (entries == 0) return tr("Archivio vuoto");
    static_assert(ARCH_MAX_ENTRIES == 64, "il testo dell'errore dice 64: aggiornalo (e lang_src.json)");
    if (entries > ARCH_MAX_ENTRIES) return tr("Troppe voci (massimo 64)");
    if ((uint64_t)cdOfs + cdSize > z.size - win + i) return tr("Archivio troncato o danneggiato");   // l'elenco deve finire prima della fine dell'archivio
    z.cdOfs = cdOfs;
    z.count = entries;
    return nullptr;
  }
  return tr("Non e un archivio zip valido");
}

// Legge la voce dell'elenco che comincia a pos e avanza pos alla prossima. Controlla tutto cio che rende una voce pericolosa o non supportata.
const char* readEntry(Zip& z, Work& w, uint32_t& pos) {
  uint8_t h[kCenLen];
  if (!z.f.seek(pos) || z.f.read(h, kCenLen) != kCenLen || le32(h) != kSigCen) return tr("Archivio troncato o danneggiato");
  uint16_t flags = le16(h + 8), nameLen = le16(h + 28), extraLen = le16(h + 30), commentLen = le16(h + 32);
  uint32_t attr = le32(h + 38);
  Entry& e = w.e;
  e.method = le16(h + 10);
  e.crc = le32(h + 16);
  e.csize = le32(h + 20);
  e.usize = le32(h + 24);
  e.localOfs = le32(h + 42);
  if ((flags & 0x2041) || le16(h + 34) || e.csize == 0xFFFFFFFFu || e.usize == 0xFFFFFFFFu || e.localOfs == 0xFFFFFFFFu)
    return tr("ZIP64, cifratura o piu volumi: non supportati");                   // 0x0001 cifrato, 0x0040 cifratura forte, 0x2000 dati mascherati
  if (nameLen == 0 || nameLen > ARCH_NAME_MAX || z.f.read((uint8_t*)w.raw, nameLen) != nameLen) return tr("Nome non sicuro: archivio rifiutato");
  pos += kCenLen + nameLen + extraLen + commentLen;
  bool dir = false;
  if (!archSafeName(w.raw, nameLen, e.name, sizeof e.name, &dir)) return tr("Nome non sicuro: archivio rifiutato");   // zip slip
  e.isDir = dir || (attr & 0x10);
  if (h[5] == 3) {                                                                // creato su Unix: i permessi stanno nei 16 bit alti
    uint16_t type = (uint16_t)((attr >> 16) & 0xF000);
    if (type == 0x4000) e.isDir = true;
    else if (type && type != 0x8000) return tr("Voce non supportata (collegamento o file speciale)");
  }
  if (e.method != 0 && e.method != 8) return tr("Metodo di compressione non supportato (solo store e deflate)");
  if (e.method == 0 && e.csize != e.usize) return tr("Archivio troncato o danneggiato");
  if (e.isDir && e.usize) return tr("Archivio troncato o danneggiato");
  if ((uint64_t)e.localOfs + kLocLen + e.csize > z.size) return tr("Archivio troncato o danneggiato");
  return nullptr;
}

// ---------- controllo di tutto l'archivio ----------

// Passa tutte le voci. dest = cartella di destinazione (nullptr = solo elenco). mask: bit i = la voce i va estratta.
// Con print scrive la tabella di "unzip -l". Ritorna false (messaggio scritto) se qualcosa non va: allora non si scrive niente.
bool checkAll(ArchCtx& c, Zip& z, Work& w, const Opts& op, const char* line, const char* dest, bool print, uint64_t& mask, unsigned& nSel) {
  uint32_t pos = z.cdOfs;
  uint64_t totalU = 0, totalC = 0;
  size_t budget = ArchCtx::budget();
  mask = 0;
  nSel = 0;
  if (print) {
    c.out().printf("Archive:  %s\r\n", op.archive.c_str());     // le righe della tabella sono in inglese, come in una shell classica
    c.out().println("  Length   Compr.    CRC-32  Name");
    c.out().println("--------  -------  --------  ----");
  }
  for (unsigned i = 0; i < z.count; i++) {
    if (const char* why = readEntry(z, w, pos)) { c.fail(op.archive.c_str(), why); return false; }
    const Entry& e = w.e;
    if (e.usize > ARCH_MAX_ENTRY) { c.fail(e.name, trf("Voce troppo grande (massimo %u KB)", (unsigned)(ARCH_MAX_ENTRY / 1024))); return false; }
    if (e.usize > ARCH_RATIO_FROM && e.usize / ARCH_MAX_RATIO > e.csize) { c.fail(e.name, trf("Rapporto di espansione sopra %u a 1: possibile zip bomb", (unsigned)ARCH_MAX_RATIO)); return false; }
    totalU += e.usize;
    totalC += e.csize;
    if (dest && e.name[0]) {
      if (!archTarget(dest, e.name, w.target)) { c.fail(e.name, tr("Nome non sicuro: archivio rifiutato")); return false; }
      if (fsProtected(w.target)) { c.fail(e.name, tr("File di sistema protetto")); return false; }
    }
    if (print && e.name[0]) c.out().printf("%8lu %8lu  %08lx  %s%s\r\n", (unsigned long)e.usize, (unsigned long)e.csize, (unsigned long)e.crc, e.name, e.isDir ? "/" : "");
    if (e.name[0] && archSelected(line, op.firstFilter, e.name)) { mask |= (uint64_t)1 << i; nSel++; }
  }
  if (totalU > budget) { c.fail(op.archive.c_str(), trf("Troppo grande dopo l'espansione (massimo %u KB, o manca spazio)", (unsigned)(budget / 1024))); return false; }
  if (totalU > ARCH_RATIO_FROM && totalU / ARCH_MAX_RATIO > totalC) { c.fail(op.archive.c_str(), trf("Rapporto di espansione sopra %u a 1: possibile zip bomb", (unsigned)ARCH_MAX_RATIO)); return false; }
  if (print) {
    c.out().println("--------  -------            ----");
    c.out().printf("%8lu %8lu            %u files\r\n", (unsigned long)totalU, (unsigned long)totalC, (unsigned)z.count);
  }
  return true;
}

// ---------- estrazione ----------

// Tra le voci scelte (mask) ce n'e una compressa con deflate?
bool anyInflate(Zip& z, Work& w, uint64_t mask) {
  uint32_t pos = z.cdOfs;
  for (unsigned i = 0; i < z.count; i++) {
    if (readEntry(z, w, pos) != nullptr) return false;
    if (((mask >> i) & 1) && w.e.method == 8 && !w.e.isDir) return true;
  }
  return false;
}

enum Result { X_OK, X_SKIP, X_FAIL };

Result extractOne(ArchCtx& c, Zip& z, Work& w, const char* dest, bool overwrite, DrvInflate& inf) {
  const Entry& e = w.e;
  ErrBuf err;
  if (!archTarget(dest, e.name, w.target)) { c.fail(e.name, tr("Nome non sicuro: archivio rifiutato")); return X_FAIL; }
  if (e.isDir) {
    if (!fsMkdirs(w.target, err)) { c.fail(e.name, err.c_str()); return X_FAIL; }
    return X_OK;
  }
  snprintf(w.parent, sizeof w.parent, "%s", w.target);
  *strrchr(w.parent, '/') = 0;
  if (!fsMkdirs(w.parent, err)) { c.fail(e.name, err.c_str()); return X_FAIL; }
  if (drvFsExists(w.target)) {
    VosFile old = drvFsOpen(w.target);
    if (old && old.isDirectory()) { c.fail(e.name, tr("Esiste una cartella con questo nome")); return X_FAIL; }
    old.close();
    if (!overwrite) { c.fail(e.name, tr("Esiste gia, saltato (usa -o)")); return X_SKIP; }
  }
  // intestazione locale: serve per sapere dove cominciano i dati (nome ed extra possono essere diversi da quelli dell'elenco)
  uint8_t h[kLocLen];
  if (!z.f.seek(e.localOfs) || z.f.read(h, kLocLen) != kLocLen || le32(h) != kSigLoc || le16(h + 8) != e.method) { c.fail(e.name, tr("Archivio troncato o danneggiato")); return X_FAIL; }
  uint64_t dataOfs = (uint64_t)e.localOfs + kLocLen + le16(h + 26) + le16(h + 28);
  if (dataOfs + e.csize > z.size || !z.f.seek((size_t)dataOfs)) { c.fail(e.name, tr("Archivio troncato o danneggiato")); return X_FAIL; }

  ArchFileOut out;
  if (!out.begin(w.target, overwrite, e.usize, e.usize, err)) { c.fail(e.name, err.c_str()); return X_FAIL; }
  DrvDefErr de = DDF_OK;
  bool done = (e.method == 0);                         // store: non c'e un fine flusso da aspettare
  if (e.method == 8) inf.restart();
  uint32_t rem = e.csize;
  while (de == DDF_OK && rem) {
    size_t n = z.f.read(w.io.b, rem < sizeof w.io.b ? rem : sizeof w.io.b);
    if (n == 0) { de = DDF_BAD_DATA; break; }
    rem -= (uint32_t)n;
    if (e.method == 0) {
      de = out.put(w.io.b, n) ? c.guard().tick(n) : DDF_SINK;
    } else if (!done) {
      size_t used = 0;
      de = inf.push(w.io.b, n, &used, ArchFileOut::sink, &out, &c.guard());
      if (de == DDF_DONE) { done = true; de = DDF_OK; }
    }
  }
  if (de == DDF_OK && !done) de = DDF_BAD_DATA;        // il flusso non si e chiuso: dati troncati
  if (de != DDF_OK) { out.abort(); c.failDef(e.name, de, out.why()); return X_FAIL; }
  if (out.crc() != e.crc) { out.abort(); c.fail(e.name, tr("CRC diverso: file danneggiato")); return X_FAIL; }
  if (!out.finish(err)) { c.fail(e.name, err.c_str()); return X_FAIL; }
  c.out().printf("%s: %s\r\n", e.method == 0 ? " extracting" : "  inflating", e.name);
  return X_OK;
}

// ---------- opzioni ----------

bool parseArgs(const char* line, OutSink& o, Opts& op) {
  for (int i = 1; i < 40; i++) {
    Word w = argAt(line, i);
    if (w.empty()) break;
    if (w.cut()) { o.println(tr("Percorso non valido")); return false; }
    if (!op.archive.empty()) { op.firstFilter = i; break; }                        // dopo l'archivio, solo filtri
    if (!isShortOpt(w)) { op.archive = w; continue; }
    for (const char* p = w.c_str() + 1; *p; p++) {
      if (*p == 'l') op.list = true;
      else if (*p == 'o') op.overwrite = true;
      else if (*p == 'd') {
        op.dir = p[1] ? Word(p + 1, strlen(p + 1)) : argAt(line, ++i);               // -dcartella oppure -d cartella
        if (op.dir.empty()) { o.println(trf("unzip: %s", tr("-d vuole il nome di una cartella"))); return false; }
        break;
      }
      else { o.println(trf("unzip: opzione non valida -- '%c'", *p)); cmdUsage(o, "unzip"); return false; }
    }
  }
  if (op.archive.empty()) { cmdUsage(o, "unzip"); return false; }
  return true;
}

}  // namespace

void shUnzip(const char* line, OutSink& o, const ShellSess& ss, uint32_t sess) {
  Opts op;
  if (!parseArgs(line, o, op)) return;
  ArchCtx c("unzip", o, sess);
  char path[FS_PATH_MAX + 1], dest[FS_PATH_MAX + 1];
  if (!shellResolvePath(ss.cwd, op.archive.c_str(), path, sizeof path)) { c.fail(op.archive.c_str(), tr("Percorso non valido")); return; }
  if (!op.list) {
    if (op.dir.empty()) snprintf(dest, sizeof dest, "%s", ss.cwd);
    else if (!shellResolvePath(ss.cwd, op.dir.c_str(), dest, sizeof dest)) { c.fail(op.dir.c_str(), tr("Percorso non valido")); return; }
    if (strcmp(dest, "/") == 0) { c.fail("", tr("Non si estrae nella radice: scegli una cartella con -d")); return; }
    if (fsProtected(dest)) { c.fail(op.dir.c_str(), tr("File di sistema protetto")); return; }
  }
  if (fsProtected(path)) { c.fail(op.archive.c_str(), tr("File di sistema protetto")); return; }

  Zip z;
  z.f = drvFsOpen(path, "r");
  if (!z.f) { c.fail(op.archive.c_str(), tr("File non trovato")); return; }
  if (z.f.isDirectory()) { c.fail(op.archive.c_str(), tr("E una cartella, ignorata")); return; }
  z.size = z.f.size();
  if (z.size > c.lim().maxIn) { c.tooBig(op.archive.c_str(), c.lim().maxIn); return; }
  if (!c.begin()) return;
  ArchHeap<Work> w;
  if (!w) { c.fail("", tr("Memoria insufficiente")); return; }
  if (const char* why = openZip(z, *w)) { c.fail(op.archive.c_str(), why); return; }

  uint64_t mask = 0;
  unsigned nSel = 0;
  if (!checkAll(c, z, *w, op, line, op.list ? nullptr : dest, op.list, mask, nSel) || op.list) return;
  if (nSel == 0) { c.fail(op.archive.c_str(), tr("Nessuna voce corrisponde")); return; }

  ErrBuf err;
  if (!fsMkdirs(dest, err)) { c.fail(dest, err.c_str()); return; }
  DrvInflate inf;
  if (anyInflate(z, *w, mask)) {                                         // la PSRAM serve solo se c'e almeno una voce compressa da estrarre
    DrvDefErr de = inf.open();
    if (de != DDF_OK) { c.failDef(op.archive.c_str(), de, nullptr); return; }
  }
  uint32_t pos = z.cdOfs;
  unsigned done = 0, skipped = 0;
  for (unsigned i = 0; i < z.count && !c.stopped(); i++) {
    if (readEntry(z, *w, pos) != nullptr) break;                         // gia controllata: non puo succedere
    if (!((mask >> i) & 1)) continue;
    Result r = extractOne(c, z, *w, dest, op.overwrite, inf);
    if (r == X_OK) done++;
    else if (r == X_SKIP) skipped++;
    else return;                                                         // un errore ferma tutto: i file gia estratti restano interi
  }
  o.println(trf("%s: %u estratti, %u saltati", "unzip", done, skipped));
}
