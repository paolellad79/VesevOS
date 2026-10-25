// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_shell_gzip.cpp
// Comandi gzip, gunzip e zcat in stile shell classica: gzip [-d] [-c] [-k] [-f] [-1..-9] file...   gunzip [-c] [-k] [-f] file.gz...   zcat file.gz...
// Come nel gzip classico, gzip comprime in "file.gz" e a successo TOGLIE l'originale (salvo -k); gunzip fa il contrario.
// FORMATO scritto (leggibile dai programmi gzip comuni): magic 1f 8b, metodo 8 (deflate), FLG = 0 (nessun nome dentro), MTIME = 0 (niente ora: l'archivio
// e riproducibile e non rivela quando e stato fatto), XFL = 2 con -9 e 4 con -1 (0 negli altri casi), OS = 3 (Unix: i nomi usano '/'),
// poi il flusso deflate, poi CRC32 e ISIZE (little endian). Un solo membro: i gzip concatenati non si leggono.
// LETTURA: si controllano magic, metodo, CRC32 e ISIZE; il CRC32 e la dimensione stanno in coda al file, quindi si da' al decompressore tutto
// tranne gli ultimi 8 byte (cosi non serve sapere dove finisce il flusso). L'uscita va in un file temporaneo e si scambia solo se tutto torna.
// -c (e zcat) stampa a schermo SOLO se il risultato e testo ASCII e al massimo ARCH_TEXT_MAX byte: la shell e testuale, non emette binari.
// La compressione usa lo stato di tdefl nella PSRAM (circa 168 KB) preso per file e rilasciato sempre. Limiti di tempo e dimensione: vos_shell_arch.h.
#include "vos_shell_arch.h"

using namespace shx;

namespace {

enum Mode { M_PACK, M_UNPACK, M_CAT };

struct Opts {
  bool decomp = false, toOut = false, keep = false, force = false;
  int  level = 6;                       // come gzip: 6 di norma
};

const size_t kGzHeader = 10;
const size_t kGzTrailer = 8;

void putLe32(uint8_t* p, uint32_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24); }
uint32_t getLe32(const uint8_t* p) { return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24); }

bool endsWith(const char* s, const char* suffix) {
  size_t n = strlen(s), m = strlen(suffix);
  return n >= m && strcmp(s + n - m, suffix) == 0;
}

// Opzioni brevi (anche unite: -dk, -9f). Ritorna false (messaggio scritto) per una sigla che non si conosce.
bool parseOpts(const char* cmd, const char* line, Mode mode, Opts& op, OutSink& o) {
  op.decomp = (mode != M_PACK);
  op.toOut = (mode == M_CAT);
  for (int i = 1; i < 24; i++) {
    Word w = argAt(line, i);
    if (w.empty()) break;
    if (w == "--") break;
    if (!isShortOpt(w)) continue;
    for (const char* p = w.c_str() + 1; *p; p++) {
      if (*p == 'd') op.decomp = true;
      else if (*p == 'c') op.toOut = true;
      else if (*p == 'k') op.keep = true;
      else if (*p == 'f') op.force = true;
      else if (*p >= '1' && *p <= '9') op.level = *p - '0';
      else { o.println(trf("%s: opzione non valida -- '%c'", cmd, *p)); cmdUsage(o, cmd); return false; }
    }
  }
  return true;
}

// ---------- intestazione gzip ----------

// Legge l'intestazione dai primi n byte del file. hdrLen = byte dell'intestazione (campi opzionali compresi).
bool parseGzHeader(const uint8_t* b, size_t n, size_t* hdrLen) {
  if (n < kGzHeader || b[0] != 0x1f || b[1] != 0x8b || b[2] != 8) return false;
  uint8_t flg = b[3];
  if (flg & 0xE0) return false;                       // bit riservati
  size_t pos = kGzHeader;
  if (flg & 4) {                                      // FEXTRA
    if (pos + 2 > n) return false;
    pos += 2 + (size_t)(b[pos] | (b[pos + 1] << 8));
  }
  for (uint8_t bit = 8; bit <= 16; bit <<= 1) {       // FNAME (8) e FCOMMENT (16): testo chiuso da uno zero
    if (!(flg & bit)) continue;
    while (pos < n && b[pos]) pos++;
    pos++;
  }
  if (flg & 2) pos += 2;                              // FHCRC
  if (pos > n) return false;                          // l'intestazione non sta nei primi byte letti
  *hdrLen = pos;
  return true;
}

// ---------- uscita a schermo (solo testo) ----------

struct TextSink {
  HeapBuf buf{ARCH_TEXT_MAX};
  bool    binary = false;
};

bool textSink(const uint8_t* d, size_t n, void* user) {
  TextSink* t = static_cast<TextSink*>(user);
  for (size_t i = 0; i < n; i++) {
    uint8_t c = d[i];
    if (!((c >= 32 && c < 127) || c == '\n' || c == '\r' || c == '\t')) { t->binary = true; return false; }   // niente ESC ne' byte alti: nessuna sequenza per il terminale
  }
  return t->buf.add((const char*)d, n);
}

// Il testo con "\r\n" a ogni riga (il terminale lo vuole), chiuso da un a-capo
void printText(OutSink& o, const char* s, size_t n) {
  char prev = 0;
  for (size_t i = 0; i < n; i++) {
    if (s[i] == '\n') { if (prev != '\r') o.print('\r'); o.print('\n'); }
    else o.print(s[i]);
    prev = s[i];
  }
  if (n && s[n - 1] != '\n') o.println();
}

// ---------- compressione ----------

// Compone nel file "path.gz". Ritorna true se tutto e andato bene (e l'originale e stato tolto, salvo -k).
bool packOne(ArchCtx& c, const Opts& op, const char* path, const char* shown) {
  if (fsProtected(path)) { c.fail(shown, tr("File di sistema protetto")); return false; }
  VosFile in = drvFsOpen(path, "r");
  if (!in) { c.fail(shown, tr("File non trovato")); return false; }
  if (in.isDirectory()) { c.fail(shown, tr("E una cartella, ignorata")); return false; }
  if (endsWith(path, ".gz")) { c.fail(shown, tr("Ha gia il suffisso .gz, ignorato")); return false; }
  size_t size = in.size();
  if (size > c.lim().maxIn) { c.tooBig(shown, c.lim().maxIn); return false; }
  char dest[FS_PATH_MAX + 1];
  if (strlen(path) + 3 > FS_PATH_MAX) { c.fail(shown, tr("Percorso non valido")); return false; }
  snprintf(dest, sizeof dest, "%s.gz", path);
  if (!op.force && drvFsExists(dest)) { c.fail(shown, tr("Esiste gia, non sovrascritto (usa -f)")); return false; }

  ArchHeap<ArchIo> io;
  if (!io) { c.fail(shown, tr("Memoria insufficiente")); return false; }
  ErrBuf err;
  ArchFileOut out;
  if (!out.begin(dest, op.force, (size_t)-1, size + size / 64 + 4096, err)) { c.fail(shown, err.c_str()); return false; }
  DrvDeflate def;
  DrvDefErr e = def.open(op.level, ArchFileOut::sink, &out);
  if (e != DDF_OK) { out.abort(); c.failDef(shown, e, nullptr); return false; }

  uint8_t hdr[kGzHeader] = {0x1f, 0x8b, 8, 0, 0, 0, 0, 0, (uint8_t)(op.level == 9 ? 2 : op.level == 1 ? 4 : 0), 3};   // MTIME = 0, OS = 3 (Unix)
  e = out.put(hdr, sizeof hdr) ? DDF_OK : DDF_SINK;
  size_t n;
  while (e == DDF_OK && (n = in.read(io->b, sizeof io->b)) > 0) e = def.push(io->b, n, &c.guard());
  if (e == DDF_OK) e = def.finish(&c.guard());
  uint8_t tail[kGzTrailer];
  putLe32(tail, def.crc());
  putLe32(tail + 4, (uint32_t)def.inBytes());
  if (e == DDF_OK && !out.put(tail, sizeof tail)) e = DDF_SINK;
  if (e == DDF_OK && def.inBytes() != size) { c.fail(shown, tr("Il file e cambiato durante la lettura")); out.abort(); return false; }
  if (e != DDF_OK) { out.abort(); c.failDef(shown, e, out.why()); return false; }
  size_t packed = out.bytes();
  if (!out.finish(err)) { c.fail(shown, err.c_str()); return false; }
  in.close();
  c.out().println(trf("%s -> %s (%u -> %u byte)", shown, dest, (unsigned)size, (unsigned)packed));
  if (!op.keep && !fsRemove(path, err)) c.fail(shown, tr("Creato il .gz ma non riesco a togliere l'originale"));
  return true;
}

// ---------- decompressione ----------

// Stato dell'alimentazione del decompressore: il flusso deflate e tutto il file tranne l'intestazione e gli ultimi 8 byte (il trailer, che finisce in tail).
struct Feed {
  DrvInflate*  inf;
  ArchCtx*     ctx;
  DrvDefSink   sink;
  void*        sinkUser;
  size_t       payloadEnd;       // offset dove finisce il flusso deflate (dimensione - 8)
  uint8_t      tail[kGzTrailer];
  bool         done = false;     // il flusso deflate e finito
  bool         extra = false;    // c'erano ancora dati dopo la fine del flusso
};

// Tratta il pezzo b[0..n) che comincia all'offset base del file; hdrLen = dove comincia il flusso.
DrvDefErr feedChunk(Feed& fd, const uint8_t* b, size_t n, size_t base, size_t hdrLen) {
  for (size_t i = 0; i < n; i++) {                    // la coda del file
    size_t abs = base + i;
    if (abs >= fd.payloadEnd && abs - fd.payloadEnd < kGzTrailer) fd.tail[abs - fd.payloadEnd] = b[i];
  }
  size_t from = hdrLen > base ? hdrLen - base : 0;
  size_t to = fd.payloadEnd > base ? fd.payloadEnd - base : 0;
  if (to > n) to = n;
  if (from >= to) return DDF_OK;
  if (fd.done) { fd.extra = true; return DDF_OK; }
  size_t used = 0;
  DrvDefErr e = fd.inf->push(b + from, to - from, &used, fd.sink, fd.sinkUser, &fd.ctx->guard());
  if (e == DDF_DONE) { fd.done = true; if (used < to - from) fd.extra = true; return DDF_OK; }
  return e;
}

bool unpackOne(ArchCtx& c, const Opts& op, const char* path, const char* shown) {
  bool toScreen = op.toOut;
  if (fsProtected(path)) { c.fail(shown, tr("File di sistema protetto")); return false; }
  VosFile in = drvFsOpen(path, "r");
  if (!in) { c.fail(shown, tr("File non trovato")); return false; }
  if (in.isDirectory()) { c.fail(shown, tr("E una cartella, ignorata")); return false; }
  if (!toScreen && !endsWith(path, ".gz")) { c.fail(shown, tr("Suffisso sconosciuto (serve .gz), ignorato")); return false; }
  size_t size = in.size();
  if (size > c.lim().maxIn) { c.tooBig(shown, c.lim().maxIn); return false; }
  if (size < kGzHeader + kGzTrailer) { c.fail(shown, tr("Non e un file gzip valido")); return false; }
  char dest[FS_PATH_MAX + 1] = "";
  if (!toScreen) {                                    // destinazione = nome senza ".gz"
    snprintf(dest, sizeof dest, "%s", path);
    dest[strlen(dest) - 3] = 0;
    if (!op.force && drvFsExists(dest)) { c.fail(shown, tr("Esiste gia, non sovrascritto (usa -f)")); return false; }
  }

  ArchHeap<ArchIo> io;
  if (!io) { c.fail(shown, tr("Memoria insufficiente")); return false; }
  size_t got = in.read(io->b, sizeof io->b);
  size_t hdrLen = 0;
  if (!parseGzHeader(io->b, got, &hdrLen) || hdrLen + kGzTrailer > size) { c.fail(shown, tr("Non e un file gzip valido")); return false; }

  ErrBuf err;
  ArchFileOut out;
  TextSink text;
  Feed fd = {};
  DrvInflate inf;
  fd.inf = &inf;
  fd.ctx = &c;
  fd.payloadEnd = size - kGzTrailer;
  if (toScreen) { fd.sink = textSink; fd.sinkUser = &text; }
  else {
    size_t room = ArchCtx::budget();
    if (room == 0 || !out.begin(dest, op.force, (size_t)-1, room, err)) { c.fail(shown, room == 0 ? tr("Spazio esaurito") : err.c_str()); return false; }
    fd.sink = ArchFileOut::sink;
    fd.sinkUser = &out;
  }
  DrvDefErr e = inf.open();
  if (e != DDF_OK) { out.abort(); c.failDef(shown, e, nullptr); return false; }

  size_t base = 0;
  while (e == DDF_OK && got > 0) {
    e = feedChunk(fd, io->b, got, base, hdrLen);
    base += got;
    got = e == DDF_OK ? in.read(io->b, sizeof io->b) : 0;
  }
  if (e != DDF_OK) {
    out.abort();
    if (e == DDF_SINK && toScreen) c.fail(shown, trf("Non e testo ASCII o supera %u KB: usa gunzip", (unsigned)(ARCH_TEXT_MAX / 1024)));
    else c.failDef(shown, e, out.why());
    return false;
  }
  if (!fd.done) { out.abort(); c.fail(shown, tr("File incompleto (troncato)")); return false; }
  if (fd.extra) { out.abort(); c.fail(shown, tr("Dati dopo la fine del flusso (gzip a piu parti non supportato)")); return false; }
  if (getLe32(fd.tail) != inf.crc()) { out.abort(); c.fail(shown, tr("CRC diverso: file danneggiato")); return false; }
  if (getLe32(fd.tail + 4) != (uint32_t)inf.outBytes()) { out.abort(); c.fail(shown, tr("Dimensione diversa: file danneggiato")); return false; }

  if (toScreen) { printText(c.out(), text.buf.c_str(), text.buf.length()); return true; }
  size_t unpacked = out.bytes();
  if (!out.finish(err)) { c.fail(shown, err.c_str()); return false; }
  in.close();
  c.out().println(trf("%s -> %s (%u -> %u byte)", shown, dest, (unsigned)size, (unsigned)unpacked));
  if (!op.keep && !fsRemove(path, err)) c.fail(shown, tr("Creato il file ma non riesco a togliere il .gz"));
  return true;
}

}  // namespace

// cmd e il nome ufficiale: "gzip", "gunzip" o "zcat"
void shGzip(const char* cmd, const char* line, OutSink& o, const ShellSess& ss, uint32_t sess) {
  Mode mode = strcmp(cmd, "gzip") == 0 ? M_PACK : strcmp(cmd, "gunzip") == 0 ? M_UNPACK : M_CAT;
  Opts op;
  if (!parseOpts(cmd, line, mode, op, o)) return;
  if (mode == M_PACK && op.toOut && !op.decomp) { o.println(trf("%s: %s", cmd, tr("Con -c si stampa solo testo: i dati compressi sono binari. Usa -d per leggere un .gz"))); return; }
  ArchCtx c(cmd, o, sess);
  int files = 0;
  for (int i = 1; i < 24; i++) {
    Word w = argAt(line, i);
    if (w.empty()) break;
    if (isShortOpt(w) || w == "--") continue;
    if (w.cut()) { o.println(tr("Percorso non valido")); continue; }
    if (!files && !c.begin()) return;
    files++;
    char path[FS_PATH_MAX + 1];
    if (!shellResolvePath(ss.cwd, w.c_str(), path, sizeof path)) { c.fail(w.c_str(), tr("Percorso non valido")); continue; }
    if (op.decomp) unpackOne(c, op, path, w.c_str());
    else packOne(c, op, path, w.c_str());
    if (c.stopped()) break;                           // Ctrl+C o tempo scaduto: i file che restano non si toccano
  }
  if (!files) { o.println(trf("%s: %s", cmd, tr("Serve almeno un file"))); cmdUsage(o, cmd); }
}
