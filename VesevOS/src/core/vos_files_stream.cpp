// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_files_stream.cpp
// Scrittura a pezzi con temporaneo e scambio (vedi vos_files_stream.h).
#include "vos_files_stream.h"
#include "vos_i18n.h"
#include "vos_lock.h"
#include "../drivers/vos_drv_fs.h"

namespace {
const size_t kSpaceMargin = 8192;       // come fsWriteText: spazio da lasciare sempre libero
}

size_t fsFreeBytes() {
  size_t total = drvFsTotal(), used = drvFsUsed();
  return total > used ? total - used : 0;
}

bool fsMkdirs(const char* cleanDir, ErrBuf& err) {
  if (!cleanDir || cleanDir[0] != '/') { err = tr("Percorso non valido"); return false; }
  char part[FS_PATH_MAX + 1];
  size_t len = strlen(cleanDir);
  if (len > FS_PATH_MAX) { err = tr("Percorso non valido"); return false; }
  for (size_t i = 1; i <= len; i++) {
    if (cleanDir[i] != '/' && cleanDir[i] != 0) continue;
    if (i == 1) continue;                                   // "//" non puo succedere con un percorso pulito
    memcpy(part, cleanDir, i);
    part[i] = 0;
    if (drvFsExists(part)) {
      VosFile d = drvFsOpen(part);
      if (!d || !d.isDirectory()) { err = tr("Esiste un file con lo stesso nome della cartella"); return false; }
      continue;
    }
    if (!fsMkdir(part, err)) return false;
  }
  return true;
}

bool FsAtomicWriter::begin(const char* path, bool overwrite, size_t maxBytes, ErrBuf& err) {
  abort();
  if (!path || !fsCleanTo(path, dst_, sizeof dst_)) { err = tr("Percorso non valido"); return false; }
  size_t plen = strlen(dst_);
  if (plen == 1) { err = tr("Nome file mancante"); return false; }
  if (plen >= 5 && !strcmp(dst_ + plen - 5, ".tmp~")) { err = tr("Nome non valido"); return false; }
  if (fsProtected(dst_)) { err = tr("File di sistema protetto"); return false; }
  bool exists = drvFsExists(dst_);
  if (exists) {
    VosFile d = drvFsOpen(dst_);
    bool isDir = d && d.isDirectory();
    d.close();
    if (isDir) { err = tr("Esiste una cartella con questo nome"); return false; }
    if (!overwrite) { err = tr("Esiste gia"); return false; }
  }
  char parent[FS_PATH_MAX + 1];
  snprintf(parent, sizeof parent, "%s", dst_);
  char* ls = strrchr(parent, '/');
  if (ls > parent) {                                         // la cartella che la contiene (se non e la radice) deve esistere
    *ls = 0;
    if (!drvFsExists(parent)) { err = tr("Cartella non trovata"); return false; }
  }
  if (const char* why = fsRoomFor(dst_)) { err = why; return false; }   // posto per una voce in piu (sovrascrivere non ne aggiunge)
  size_t freeB = fsFreeBytes();
  if (freeB <= kSpaceMargin) { err = tr("Spazio esaurito"); return false; }
  max_ = freeB - kSpaceMargin;
  if (maxBytes < max_) max_ = maxBytes;
  snprintf(tmp_, sizeof tmp_, "%s.tmp~", dst_);
  fsSaveMutex().lock();                                      // fino a commit() o abort(): la pulizia dei .tmp~ non deve toccare il nostro temporaneo
  locked_ = true;
  f_ = drvFsOpen(tmp_, "w");
  if (!f_) { fsSaveMutex().unlock(); locked_ = false; err = tr("Impossibile scrivere"); return false; }
  open_ = true;
  n_ = 0;
  full_ = false;
  return true;
}

bool FsAtomicWriter::write(const uint8_t* data, size_t n) {
  if (!open_) return false;
  if (n_ + n > max_) { full_ = true; return false; }
  if (n && f_.write(data, n) != n) { full_ = true; return false; }
  n_ += n;
  return true;
}

bool FsAtomicWriter::patch(size_t pos, const uint8_t* data, size_t n) {
  if (!open_ || pos > n_ || n > n_ - pos) return false;
  if (!f_.seek(pos) || f_.write(data, n) != n) return false;
  return f_.seek(n_);
}

bool FsAtomicWriter::commit(ErrBuf& err) {
  if (!open_) { err = tr("Impossibile scrivere"); return false; }
  f_.close();
  // rename() su LittleFS sostituisce il file che c'e in un solo passo: non c'e un momento in cui il file manca (stessa idea di fsWriteText).
  bool ok = drvFsRename(tmp_, dst_);
  if (!ok) { drvFsRemove(tmp_); err = tr("Impossibile scrivere"); }
  open_ = false;
  if (locked_) { fsSaveMutex().unlock(); locked_ = false; }
  return ok;
}

void FsAtomicWriter::abort() {
  if (open_) {
    f_.close();
    drvFsRemove(tmp_);
    open_ = false;
  }
  if (locked_) { fsSaveMutex().unlock(); locked_ = false; }
}
