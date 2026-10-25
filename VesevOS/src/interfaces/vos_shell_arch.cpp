// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_shell_arch.cpp
// Pezzi in comune dei comandi gzip, gunzip, zcat, unzip e tar (vedi vos_shell_arch.h per limiti e protezioni).
#include "vos_shell_arch.h"
#include "../drivers/vos_drv_console.h"
#include "../drivers/vos_drv_fs.h"
#include "../core/vos_time.h"
#include <time.h>
#include <strings.h>

using namespace shx;

namespace {
const uint32_t kSerialMs = 120000;        // tempo massimo dalla seriale
const uint32_t kWebMs = 10000;            // tempo massimo dalla pagina web (e da script e regole): il task httpd serve tutti e il watchdog e a 30 s
const size_t   kSerialIn = 1048576;       // la memoria interna e di 640 KB: dalla seriale non c'e un tetto vero
const size_t   kWebIn = 131072;
}  // namespace

ArchLimits shx::archLimits(uint32_t sess) {
  if (sess == SESS_SERIAL) return {kSerialMs, kSerialIn, true};
  return {kWebMs, kWebIn, false};
}

bool shx::archSerialCancel(void*) {
  bool stop = false;
  while (drvConAvailable() > 0)
    if (drvConRead() == 3) stop = true;
  return stop;
}

// ---------- contesto ----------

ArchCtx::ArchCtx(const char* cmd, OutSink& o, uint32_t sess) : cmd_(cmd), o_(o), lim_(archLimits(sess)) {}

bool ArchCtx::begin() {
  if (!lock_.ok()) { o_.println(trf("%s: %s", cmd_, tr("Occupato: un'altra compressione o estrazione e in corso"))); return false; }
  guard_.start(lim_.maxMs, lim_.serial ? archSerialCancel : nullptr, nullptr);
  return true;
}

void ArchCtx::fail(const char* name, const char* why) {
  if (name && name[0]) o_.println(trf("%s: %s: %s", cmd_, name, why));
  else o_.println(trf("%s: %s", cmd_, why));
}

// Dalla pagina web i limiti sono piu bassi: il consiglio dice dove andare per i file grandi.
void ArchCtx::webHint() {
  if (lim_.serial) return;
  o_.println(trf("Dalla pagina web il limite e %u KB e %u s: per file piu grandi o piu lenti usa la seriale", (unsigned)(lim_.maxIn / 1024), (unsigned)(lim_.maxMs / 1000)));
}

void ArchCtx::tooBig(const char* name, size_t max) {
  fail(name, trf("Troppo grande (massimo %u KB)", (unsigned)(max / 1024)));
  webHint();
}

void ArchCtx::failDef(const char* name, DrvDefErr e, const char* sinkWhy) {
  switch (e) {
    case DDF_NO_PSRAM: fail(name, tr("Serve la PSRAM: questa scheda non ce l'ha")); break;
    case DDF_NO_MEM:   fail(name, tr("Memoria insufficiente")); break;
    case DDF_BAD_DATA: fail(name, tr("Dati compressi non validi")); break;
    case DDF_SINK:     fail(name, sinkWhy ? sinkWhy : tr("Spazio esaurito")); break;
    case DDF_CANCEL:   fail(name, tr("Interrotto (Ctrl+C)")); stopped_ = true; break;
    case DDF_TIMEOUT:  fail(name, trf("Tempo massimo superato (%u s)", (unsigned)(lim_.maxMs / 1000))); webHint(); stopped_ = true; break;
    default:           fail(name, tr("Operazione non riuscita")); break;
  }
}

size_t ArchCtx::budget() {
  size_t freeB = fsFreeBytes();
  size_t room = freeB > ARCH_SPACE_KEEP ? freeB - ARCH_SPACE_KEEP : 0;
  return room < ARCH_MAX_TOTAL ? room : ARCH_MAX_TOTAL;
}

// ---------- archivi che si creano ----------

const char* shx::archRelName(const char* base, const char* abs, bool& stripped) {
  size_t bl = strlen(base);
  if (strcmp(abs, base) == 0) return "";
  if (bl == 1) return abs + 1;
  if (strncmp(abs, base, bl) == 0 && abs[bl] == '/') return abs + bl + 1;
  stripped = true;
  return abs + 1;
}

bool shx::archIsSecret(const char* path) {
  if (fsProtected(path)) return true;
  static const char* const kExt[] = {".pem", ".key", ".crt", ".der", ".p12", ".vbk"};
  size_t n = strlen(path);
  for (const char* ext : kExt) {
    size_t m = strlen(ext);
    if (n >= m && strcasecmp(path + n - m, ext) == 0) return true;
  }
  return false;
}

bool shx::archIsHidden(const char* name) {
  size_t n = strlen(name);
  return name[0] == '.' || (n >= 5 && strcmp(name + n - 5, ".tmp~") == 0);
}

bool shx::archReadKids(VosFile& dir, ArchKids& k) {
  for (VosFile ch = dir.openNextFile(); ch; ch = dir.openNextFile()) {
    if (k.count >= ARCH_KIDS_MAX) return false;
    snprintf(k.name[k.count++], ARCH_NAME_MAX + 1, "%s", ch.name());
  }
  return true;
}

void shx::archDosStamp(uint16_t* dosTime, uint16_t* dosDate) {
  *dosTime = 0;
  *dosDate = (uint16_t)((0 << 9) | (1 << 5) | 1);                   // 1980-01-01 00:00: l'inizio del formato DOS
  if (!timeValid()) return;
  time_t now = time(nullptr);
  struct tm t;
  if (!localtime_r(&now, &t) || t.tm_year < 80 || t.tm_year > 207) return;   // il formato DOS va dal 1980 al 2107
  *dosTime = (uint16_t)((t.tm_hour << 11) | (t.tm_min << 5) | (t.tm_sec / 2));
  *dosDate = (uint16_t)(((t.tm_year - 80) << 9) | ((t.tm_mon + 1) << 5) | t.tm_mday);
}

// ---------- nomi sicuri ----------

bool shx::archSafeName(const char* raw, size_t len, char* out, size_t cap, bool* isDir) {
  out[0] = 0;
  *isDir = false;
  if (len == 0 || len > ARCH_NAME_MAX || cap < ARCH_NAME_MAX + 1) return false;
  if (raw[0] == '/') return false;                                  // percorso assoluto
  for (size_t i = 0; i < len; i++) {
    unsigned char c = (unsigned char)raw[i];
    if (c < 32 || c >= 127 || c == '\\') return false;              // controllo, non ASCII, backslash di Windows
  }
  *isDir = raw[len - 1] == '/';
  size_t n = 0, i = 0;
  while (i < len) {
    size_t j = i;
    while (j < len && raw[j] != '/') j++;
    size_t seg = j - i;
    if (seg == 0) return false;                                     // "a//b"
    bool dot = (seg == 1 && raw[i] == '.');
    if (seg == 2 && raw[i] == '.' && raw[i + 1] == '.') return false;   // risalire: il cuore dello zip slip
    if (!dot) {
      if (n) out[n++] = '/';
      if (n + seg > ARCH_NAME_MAX) return false;
      memcpy(out + n, raw + i, seg);
      n += seg;
    }
    i = j + 1;
  }
  out[n] = 0;
  return true;
}

bool shx::archTarget(const char* base, const char* rel, char* out) {
  size_t bl = strlen(base);
  if (bl <= 1) return false;                                        // mai la radice: la cartella di destinazione e sempre una sottocartella
  if (!shellResolvePath(base, rel, out, FS_PATH_MAX + 1)) return false;
  return strncmp(out, base, bl) == 0 && out[bl] == '/';             // anche "~/x" o altro non deve uscire dalla destinazione
}

bool shx::archSelected(const char* line, int firstFilter, const char* name) {
  if (!firstFilter) return true;
  size_t nl = strlen(name);
  for (int i = firstFilter; i < 40; i++) {
    Word f = argAt(line, i);
    if (f.empty()) break;
    size_t fl = f.length();
    while (fl > 1 && f.c_str()[fl - 1] == '/') fl--;                // "dir/" vale come "dir"
    if (fl == nl && strncmp(f.c_str(), name, nl) == 0) return true;
    if (fl < nl && strncmp(f.c_str(), name, fl) == 0 && name[fl] == '/') return true;
  }
  return false;
}

// ---------- un file in uscita ----------

bool ArchFileOut::begin(const char* path, bool overwrite, size_t declared, size_t maxBytes, ErrBuf& err) {
  declared_ = declared;
  n_ = 0;
  crc_ = 0;
  over_ = false;
  return w_.begin(path, overwrite, maxBytes, err);
}

bool ArchFileOut::put(const uint8_t* data, size_t n) {
  if (n > declared_ - n_) { over_ = true; return false; }          // piu dati di quelli dichiarati: archivio bugiardo (zip bomb)
  if (!w_.write(data, n)) return false;
  crc_ = drvCrc32(crc_, data, n);
  n_ += n;
  return true;
}

const char* ArchFileOut::why() const {
  return over_ ? tr("Piu dati di quelli dichiarati nell'archivio") : tr("Spazio esaurito");
}

bool ArchFileOut::finish(ErrBuf& err) {
  if (declared_ != (size_t)-1 && n_ != declared_) { w_.abort(); err = tr("Dimensione diversa da quella dichiarata: archivio danneggiato"); return false; }
  return w_.commit(err);
}

// Il collegamento con la shell: i comandi di questo gruppo. Il ruolo (solo Admin) lo ha gia deciso la tabella dei ruoli.
bool shCmdArchive(const Word& c, const char* line, OutSink& o, const ShellSess& ss, uint32_t sess) {
  if (c == "gzip" || c == "gunzip" || c == "zcat") shGzip(c.c_str(), line, o, ss, sess);
  else if (c == "unzip") shUnzip(line, o, ss, sess);
  else if (c == "zip") shZip(line, o, ss, sess);
  else if (c == "tar") shTar(line, o, ss, sess);
  else return false;
  return true;
}
