// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_shell_zipc.cpp
// Comando zip in stile shell classica (Info-ZIP): zip [-r] [-0..-9] [-q] [-f] [-j] archivio.zip file...   CREA un archivio .zip nuovo da file e cartelle di LittleFS.
// (La lettura e l'estrazione sono in vos_shell_zip.cpp, il comando unzip.)
// FORMATO scritto (leggibile da unzip e dagli altri programmi): ZIP standard versione 2.0, metodi 0 (store, con -0 e per i file vuoti) e 8 (deflate,
// livelli 1-9 come gzip, 6 di norma). Per ogni voce un'intestazione locale con CRC32 e dimensioni scritti TORNANDO INDIETRO nel file (seek) dopo i dati:
// nessun "data descriptor". In fondo la directory centrale e la fine della directory. Nomi solo ASCII (bit 11 dei flag a zero), niente extra, niente
// commenti, niente ZIP64, niente cifratura. Permessi fissi (0644 file, 0755 cartelle), creato "su Unix" (i nomi usano '/').
// DATA E ORA: LittleFS non salva la data dei file. Tutte le voci portano l'ora di sistema del momento in cui si crea l'archivio, se e valida
// (NTP sincronizzato o ora impostata a mano), altrimenti 1980-01-01 00:00 (l'inizio del formato DOS).
// NON SUPPORTATO: aggiornare, togliere o aggiungere voci a un archivio che c'e gia (-u, -d, -g): si ricrea da zero con -f. Senza -f un archivio che c'e
// gia non si tocca (Info-ZIP lo aggiornerebbe: qui no).
// OPZIONI: -r scende nelle cartelle (al massimo 3 livelli), -j memorizza solo il nome del file, -q silenzio, -f sovrascrive l'archivio di destinazione.
// Senza -r una cartella entra come sola voce vuota. Con -j le cartelle non entrano.
// PROTEZIONI: i file protetti (fsProtected, chiavi, certificati, copie di configurazione: archIsSecret) non entrano MAI: nominati a mano fanno fallire
// il comando, trovati scendendo nelle cartelle si saltano con un avviso. I nomi nascosti (.xxx, .tmp~) si saltano scendendo nelle cartelle e si
// includono solo se li scrivi per nome. L'archivio non entra mai in se stesso. Nomi relativi (mai '/' iniziale, mai ".."), solo caratteri ammessi.
// TETTI: ingresso totale e file come gli altri comandi (vos_shell_arch.h: 128 KB dalla pagina web, 1 MB dalla seriale, 1 MB per file), al massimo 64 voci,
// archivio finale entro lo spazio libero meno 16 KB: se non ci sta si interrompe e il temporaneo sparisce.
// SCRITTURA: in "archivio.zip.tmp~" e scambio alla fine (FsAtomicWriter): un errore o Ctrl+C non lascia un archivio a meta.
// Il compressore (circa 168 KB di PSRAM) si prende per ogni file compresso e si rilascia subito.
#include "vos_shell_arch.h"

using namespace shx;

namespace {

const uint32_t kSigLoc = 0x04034b50, kSigCen = 0x02014b50, kSigEocd = 0x06054b50;
const size_t   kLocLen = 30, kCenLen = 46, kEocdLen = 22;
const uint16_t kVersion = 20;                         // 2.0: serve per deflate e cartelle
const uint16_t kMadeBy = (3 << 8) | 20;               // creato su Unix, versione 2.0: i permessi stanno negli attributi
const uint32_t kAttrFile = 0x81A4u << 16;             // regular file 0644
const uint32_t kAttrDir = (0x41EDu << 16) | 0x10;     // cartella 0755 e il bit "cartella" del DOS
const size_t   kExtraRoom = 32768;                    // oltre ai dati: intestazioni e directory centrale (64 voci con nomi di 100 caratteri: circa 16 KB)

static_assert(ARCH_MAX_ENTRIES == 64, "il testo dell'errore dice 64: aggiornalo (e lang_src.json)");

struct Opts {
  bool recurse = false, quiet = false, force = false, junk = false;
  int  level = 6;                                     // 0 = store
  Word archive;
  int  operands = 0;                                  // quante parole dopo l'archivio
};

struct Entry {
  char     name[ARCH_NAME_MAX + 1];                   // con '/' finale per le cartelle
  uint32_t crc, csize, usize, ofs;
  uint16_t method;
  bool     dir;
};

// Memoria di lavoro, sull'heap (lo stack del server web e piccolo)
struct Work {
  Entry   ent[ARCH_MAX_ENTRIES];
  ArchIo  io;
  uint8_t hb[kCenLen + ARCH_NAME_MAX + 8];            // un'intestazione (locale o centrale) con il suo nome
};

struct Build {
  Build(ArchCtx& ctx, Work& work, const Opts& o, const char* archivePath) : c(ctx), w(work), op(o), archive(archivePath) {
    snprintf(archiveTmp, sizeof archiveTmp, "%s.tmp~", archivePath);
  }
  ArchCtx&     c;
  Work&        w;
  const Opts&  op;
  ArchFileOut  out;
  const char*  archive;                               // percorso risolto dell'archivio (da non archiviare)
  char         archiveTmp[FS_PATH_MAX + 8];           // e il suo temporaneo
  unsigned     count = 0;                             // voci scritte
  size_t       inBytes = 0;                           // byte dei file letti finora
  uint16_t     dosTime = 0, dosDate = 0;
  bool         fatal = false;                         // un errore che ferma tutto
};

bool stop(Build& b, const char* name, const char* why) {
  b.c.fail(name, why);
  b.fatal = true;
  return false;
}

// Errore del compressore o della scrittura (spazio finito, tempo, Ctrl+C...)
bool stopDef(Build& b, const char* name, DrvDefErr e) {
  b.c.failDef(name, e, b.out.why());
  b.fatal = true;
  return false;
}

bool put(Build& b, const char* name, const uint8_t* data, size_t n) {
  if (!b.out.put(data, n)) return stopDef(b, name, DDF_SINK);
  DrvDefErr g = b.c.guard().tick(n);
  return g == DDF_OK ? true : stopDef(b, name, g);
}

// ---------- intestazioni ----------

// Intestazione locale di e in b.w.hb; ritorna la lunghezza. CRC e dimensioni valgono quelli di e (zero all'inizio: si correggono dopo i dati).
size_t localHeader(Build& b, const Entry& e) {
  uint8_t* h = b.w.hb;
  size_t nl = strlen(e.name);
  archPutLe32(h, kSigLoc);
  archPutLe16(h + 4, kVersion);
  archPutLe16(h + 6, 0);                              // flag: niente cifratura, niente data descriptor, nome non UTF-8
  archPutLe16(h + 8, e.method);
  archPutLe16(h + 10, b.dosTime);
  archPutLe16(h + 12, b.dosDate);
  archPutLe32(h + 14, e.crc);
  archPutLe32(h + 18, e.csize);
  archPutLe32(h + 22, e.usize);
  archPutLe16(h + 26, (uint32_t)nl);
  archPutLe16(h + 28, 0);                             // nessun campo extra
  memcpy(h + kLocLen, e.name, nl);
  return kLocLen + nl;
}

// Intestazione della directory centrale di e in b.w.hb
size_t centralHeader(Build& b, const Entry& e) {
  uint8_t* h = b.w.hb;
  size_t nl = strlen(e.name);
  archPutLe32(h, kSigCen);
  archPutLe16(h + 4, kMadeBy);
  archPutLe16(h + 6, kVersion);
  archPutLe16(h + 8, 0);
  archPutLe16(h + 10, e.method);
  archPutLe16(h + 12, b.dosTime);
  archPutLe16(h + 14, b.dosDate);
  archPutLe32(h + 16, e.crc);
  archPutLe32(h + 20, e.csize);
  archPutLe32(h + 24, e.usize);
  archPutLe16(h + 28, (uint32_t)nl);
  archPutLe16(h + 30, 0);                             // extra
  archPutLe16(h + 32, 0);                             // commento
  archPutLe16(h + 34, 0);                             // disco
  archPutLe16(h + 36, 0);                             // attributi interni
  archPutLe32(h + 38, e.dir ? kAttrDir : kAttrFile);
  archPutLe32(h + 42, e.ofs);
  memcpy(h + kCenLen, e.name, nl);
  return kCenLen + nl;
}

// Apre una voce nuova: la registra e scrive la sua intestazione locale (con CRC e dimensioni a zero). nullptr = errore, gia detto.
Entry* beginEntry(Build& b, const char* name, bool dir, uint16_t method) {
  Entry& e = b.w.ent[b.count++];
  memset(&e, 0, sizeof e);
  snprintf(e.name, sizeof e.name, "%s", name);
  e.ofs = (uint32_t)b.out.bytes();
  e.method = method;
  e.dir = dir;
  return put(b, name, b.w.hb, localHeader(b, e)) ? &e : nullptr;
}

// Dopo i dati: torna all'intestazione locale e scrive CRC32, dimensione compressa e dimensione vera
bool patchSizes(Build& b, const Entry& e) {
  uint8_t t[12];
  archPutLe32(t, e.crc);
  archPutLe32(t + 4, e.csize);
  archPutLe32(t + 8, e.usize);
  if (!b.out.patch(e.ofs + 14, t, sizeof t)) return stop(b, e.name, tr("Impossibile scrivere"));
  return true;
}

// "  adding: nome (deflated 45%)" come il comando zip classico (le righe di programma restano in inglese, come quelle di unzip)
void say(Build& b, const Entry& e) {
  if (b.op.quiet) return;
  int pct = e.usize ? (int)(((int64_t)e.usize - (int64_t)e.csize) * 100 / (int64_t)e.usize) : 0;
  b.c.out().printf("  adding: %s (%s %d%%)\r\n", e.name, e.method == 0 ? "stored" : "deflated", pct);
}

// ---------- voci ----------

bool addDir(Build& b, const char* name) {
  Entry* e = beginEntry(b, name, true, 0);
  if (!e) return false;
  say(b, *e);
  return true;
}

bool addFile(Build& b, VosFile& f, const char* name) {
  size_t size = f.size();
  if (size > ARCH_MAX_ENTRY) { b.c.tooBig(name, ARCH_MAX_ENTRY); b.fatal = true; return false; }
  b.inBytes += size;
  if (b.inBytes > b.c.lim().maxIn) { b.c.tooBig(name, b.c.lim().maxIn); b.fatal = true; return false; }
  uint16_t method = (b.op.level == 0 || size == 0) ? 0 : 8;       // un file vuoto non si comprime (come in Info-ZIP)
  Entry* e = beginEntry(b, name, false, method);
  if (!e) return false;

  DrvDeflate def;                                                  // la PSRAM del compressore si libera all'uscita da questa funzione
  DrvDefErr de = DDF_OK;
  if (method == 8) de = def.open(b.op.level, ArchFileOut::sink, &b.out);
  uint32_t crc = 0;
  size_t read = 0, n;
  while (de == DDF_OK && (n = f.read(b.w.io.b, sizeof b.w.io.b)) > 0) {
    read += n;
    if (read > size) break;                                        // il file e cresciuto mentre lo leggevo
    if (method == 8) {
      de = def.push(b.w.io.b, n, &b.c.guard());
    } else {
      crc = drvCrc32(crc, b.w.io.b, n);
      de = b.out.put(b.w.io.b, n) ? b.c.guard().tick(n) : DDF_SINK;
    }
  }
  if (de == DDF_OK && method == 8) de = def.finish(&b.c.guard());
  if (de != DDF_OK) return stopDef(b, name, de);
  if (read != size) return stop(b, name, tr("Il file e cambiato durante la lettura"));
  e->usize = (uint32_t)size;
  e->crc = method == 8 ? def.crc() : crc;
  e->csize = method == 8 ? (uint32_t)def.outBytes() : (uint32_t)size;
  if (!patchSizes(b, *e)) return false;
  say(b, *e);
  return true;
}

// ---------- cammino nelle cartelle ----------

// Il nome che la voce avra nell'archivio. rel: percorso relativo alla cartella corrente; con -j solo l'ultimo pezzo.
// Ritorna false se il nome non e ammesso (vuoto, ".." , caratteri strani, troppo lungo). Le cartelle finiscono con '/'.
bool storedName(const Build& b, const char* abs, const char* rel, bool dir, char* out) {
  const char* raw = b.op.junk ? strrchr(abs, '/') + 1 : rel;
  char safe[ARCH_NAME_MAX + 1];
  bool isDir = false;
  if (!archSafeName(raw, strlen(raw), safe, sizeof safe, &isDir) || !safe[0]) return false;
  size_t n = strlen(safe);
  if (dir) {
    if (n + 1 > ARCH_NAME_MAX) return false;
    safe[n] = '/';
    safe[n + 1] = 0;
  }
  snprintf(out, ARCH_NAME_MAX + 1, "%s", safe);
  return true;
}

bool isDuplicate(const Build& b, const char* name) {
  for (unsigned i = 0; i < b.count; i++)
    if (strcmp(b.w.ent[i].name, name) == 0) return true;
  return false;
}

bool tooMany(Build& b) { return stop(b, "", tr("Troppe voci (massimo 64)")); }

// Aggiunge abs (file o cartella) con il nome rel. named: l'ha scritto l'utente (non l'ho trovato scendendo). rel vuoto = la cartella corrente
// stessa: non ha una voce, se ne aggiungono solo i figli. Ritorna false solo per un errore che ferma tutto.
bool addTree(Build& b, const char* abs, const char* rel, unsigned depth, bool named) {
  const char* shown = rel[0] ? rel : abs;
  const char* base = strrchr(abs, '/') + 1;                        // abs e pulito e comincia con '/'
  if (strcmp(abs, b.archive) == 0 || strcmp(abs, b.archiveTmp) == 0) { b.c.fail(shown, tr("E l'archivio stesso, saltato")); return true; }
  if (archIsSecret(abs)) {
    if (named) return stop(b, shown, tr("File di sistema protetto"));
    b.c.fail(shown, tr("File di sistema protetto, saltato"));
    return true;
  }
  if (!named && archIsHidden(base)) {
    if (!b.op.quiet) b.c.fail(shown, tr("File nascosto, saltato (scrivi il nome per intero per includerlo)"));
    return true;
  }
  VosFile f = drvFsOpen(abs, "r");
  if (!f) return stop(b, shown, tr("File non trovato"));

  char name[ARCH_NAME_MAX + 1];
  if (!f.isDirectory()) {
    if (!rel[0]) return stop(b, abs, tr("Percorso non valido"));
    if (!storedName(b, abs, rel, false, name)) { b.c.fail(rel, tr("Nome non ammesso, saltato")); return true; }
    if (isDuplicate(b, name)) { b.c.fail(rel, tr("Nome gia presente nell'archivio, saltato")); return true; }
    if (b.count >= ARCH_MAX_ENTRIES) return tooMany(b);
    return addFile(b, f, name);
  }

  if (depth > ARCH_DEPTH_MAX) return stop(b, shown, tr("Cartelle troppo profonde"));
  if (rel[0] && !b.op.junk) {                                      // con -j le cartelle non entrano
    if (!storedName(b, abs, rel, true, name)) { b.c.fail(rel, tr("Nome non ammesso, saltato")); return true; }
    if (!isDuplicate(b, name)) {
      if (b.count >= ARCH_MAX_ENTRIES) return tooMany(b);
      if (!addDir(b, name)) return false;
    }
  }
  if (!b.op.recurse) return true;
  ArchHeap<ArchKids> kids;                                         // i nomi dei figli: poi la cartella si chiude, cosi non restano aperte piu cartelle insieme
  if (!kids) return stop(b, shown, tr("Memoria insufficiente"));
  if (!archReadKids(f, *kids)) return stop(b, shown, tr("Cartella con troppe voci"));
  f.close();
  for (unsigned i = 0; i < kids->count && !b.fatal; i++) {
    char childAbs[FS_PATH_MAX + 1], childRel[ARCH_NAME_MAX + 1];
    int na = snprintf(childAbs, sizeof childAbs, "%s%s%s", abs, strcmp(abs, "/") ? "/" : "", kids->name[i]);
    int nr = snprintf(childRel, sizeof childRel, "%s%s%s", rel, rel[0] ? "/" : "", kids->name[i]);
    if (na < 0 || na > (int)FS_PATH_MAX || nr < 0 || nr > (int)ARCH_NAME_MAX) { b.c.fail(kids->name[i], tr("Nome troppo lungo (massimo 100 caratteri)")); continue; }
    if (!addTree(b, childAbs, childRel, depth + 1, false)) return false;
  }
  return true;
}

// ---------- directory centrale ----------

bool writeCentral(Build& b) {
  uint32_t cdOfs = (uint32_t)b.out.bytes();
  for (unsigned i = 0; i < b.count; i++) {
    const Entry& e = b.w.ent[i];
    if (!put(b, e.name, b.w.hb, centralHeader(b, e))) return false;
  }
  uint8_t* h = b.w.hb;
  uint32_t cdSize = (uint32_t)b.out.bytes() - cdOfs;
  memset(h, 0, kEocdLen);
  archPutLe32(h, kSigEocd);
  archPutLe16(h + 8, b.count);                                     // voci in questo disco
  archPutLe16(h + 10, b.count);                                    // voci in tutto
  archPutLe32(h + 12, cdSize);
  archPutLe32(h + 16, cdOfs);                                      // commento di lunghezza 0 (gli ultimi 2 byte)
  return put(b, "", h, kEocdLen);
}

// ---------- opzioni ----------

bool parseArgs(const char* line, OutSink& o, Opts& op) {
  for (int i = 1; i < 40; i++) {
    Word w = argAt(line, i);
    if (w.empty()) break;
    if (w.cut()) { o.println(tr("Percorso non valido")); return false; }
    if (!isShortOpt(w)) {
      if (op.archive.empty()) op.archive = w;
      else op.operands++;
      continue;
    }
    for (const char* p = w.c_str() + 1; *p; p++) {
      if (*p == 'r') op.recurse = true;
      else if (*p == 'q') op.quiet = true;
      else if (*p == 'f') op.force = true;
      else if (*p == 'j') op.junk = true;
      else if (*p >= '0' && *p <= '9') op.level = *p - '0';
      else if (*p == 'u' || *p == 'd' || *p == 'g') { o.println(trf("%s: %s", "zip", tr("Aggiornare o togliere voci di un archivio che c'e gia non e supportato: ricrealo con -f"))); return false; }
      else { o.println(trf("%s: opzione non valida -- '%c'", "zip", *p)); cmdUsage(o, "zip"); return false; }
    }
  }
  if (op.archive.empty() || op.operands == 0) { cmdUsage(o, "zip"); return false; }
  return true;
}

// Il nome ha un suffisso (un '.' dopo l'ultima '/')? Come Info-ZIP, senza suffisso si aggiunge ".zip".
bool hasSuffix(const char* s) {
  const char* dot = strrchr(s, '.');
  const char* slash = strrchr(s, '/');
  return dot && dot[1] && (!slash || dot > slash);
}

}  // namespace

void shZip(const char* line, OutSink& o, const ShellSess& ss, uint32_t sess) {
  Opts op;
  if (!parseArgs(line, o, op)) return;
  ArchCtx c("zip", o, sess);

  char want[Word::MAX + 8];                                        // il nome dell'archivio come lo vede l'utente (con ".zip" se mancava)
  snprintf(want, sizeof want, "%s%s", op.archive.c_str(), hasSuffix(op.archive.c_str()) ? "" : ".zip");
  char path[FS_PATH_MAX + 1];
  if (!shellResolvePath(ss.cwd, want, path, sizeof path)) { c.fail(want, tr("Percorso non valido")); return; }
  if (archIsSecret(path)) { c.fail(want, tr("File di sistema protetto")); return; }
  if (!op.force && drvFsExists(path)) { c.fail(want, tr("Esiste gia, non sovrascritto (usa -f)")); return; }

  if (!c.begin()) return;
  ArchHeap<Work> w;
  if (!w) { c.fail("", tr("Memoria insufficiente")); return; }
  Build b(c, *w, op, path);
  size_t freeB = fsFreeBytes();
  if (freeB <= ARCH_SPACE_KEEP) { c.fail(want, tr("Spazio esaurito")); return; }
  size_t cap = freeB - ARCH_SPACE_KEEP;                            // l'archivio finale deve stare nello spazio libero (meno il margine)
  if (cap > c.lim().maxIn + kExtraRoom) cap = c.lim().maxIn + kExtraRoom;
  ErrBuf err;
  if (!b.out.begin(path, op.force, (size_t)-1, cap, err)) { c.fail(want, err.c_str()); return; }
  archDosStamp(&b.dosTime, &b.dosDate);

  bool stripped = false, gotArchive = false;
  for (int i = 1; i < 40 && !b.fatal; i++) {
    Word wd = argAt(line, i);
    if (wd.empty()) break;
    if (isShortOpt(wd)) continue;
    if (!gotArchive) { gotArchive = true; continue; }              // la prima parola che non e un'opzione e l'archivio
    char abs[FS_PATH_MAX + 1];
    if (!shellResolvePath(ss.cwd, wd.c_str(), abs, sizeof abs)) { c.fail(wd.c_str(), tr("Percorso non valido")); b.out.abort(); return; }
    addTree(b, abs, archRelName(ss.cwd, abs, stripped), 0, true);
  }
  if (b.fatal) { b.out.abort(); return; }
  if (b.count == 0) { b.out.abort(); c.fail("", tr("Niente da fare: nessun file aggiunto")); return; }
  if (stripped && !op.quiet) c.fail("", tr("Tolgo la '/' iniziale dai nomi"));
  if (!writeCentral(b)) { b.out.abort(); return; }
  if (!b.out.finish(err)) c.fail(want, err.c_str());
}
