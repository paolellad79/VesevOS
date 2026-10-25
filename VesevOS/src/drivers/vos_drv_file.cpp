// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_drv_file.cpp
// STRATO 1 (driver): VosFile sopra POSIX/VFS. Vedi vos_drv_file.h.
#include "vos_drv_file.h"
#include "vos_drv_activity.h"
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static const size_t FILE_BUF_BYTES = 4096;   // buffer di lettura/scrittura (come faceva Arduino)

void VosFile::take(VosFile& o) {
  file_ = o.file_; dir_ = o.dir_; vfsPath_ = o.vfsPath_; entry_ = o.entry_; isDir_ = o.isDir_;
  o.file_ = nullptr; o.dir_ = nullptr; o.vfsPath_ = nullptr; o.entry_ = false; o.isDir_ = false;
}

void VosFile::close() {
  if (file_) fclose(file_);
  if (dir_) closedir(dir_);
  free(vfsPath_);
  file_ = nullptr; dir_ = nullptr; vfsPath_ = nullptr; entry_ = false; isDir_ = false;
}

const char* VosFile::name() const {
  if (!vfsPath_) return "";
  const char* slash = strrchr(vfsPath_, '/');
  return slash ? slash + 1 : vfsPath_;
}

size_t VosFile::size() const {
  if (isDir_ || !(file_ || entry_)) return 0;
  struct stat st;
  int rc = file_ ? fstat(fileno(file_), &st) : stat(vfsPath_, &st);
  return rc == 0 ? (size_t)st.st_size : 0;
}

int VosFile::available() const {
  if (!file_) return 0;
  long pos = ftell(file_);
  size_t total = size();
  return (pos >= 0 && (size_t)pos < total) ? (int)(total - (size_t)pos) : 0;
}

bool VosFile::seek(size_t pos) {
  return file_ && fseek(file_, (long)pos, SEEK_SET) == 0;
}

size_t VosFile::position() const {
  long pos = file_ ? ftell(file_) : -1;
  return pos > 0 ? (size_t)pos : 0;
}

int VosFile::read() {
  if (!file_) return -1;
  drvActAdd(ACT_FS_READ);
  return fgetc(file_);
}

size_t VosFile::read(uint8_t* buf, size_t n) {
  if (!file_ || !buf || !n) return 0;
  drvActAdd(ACT_FS_READ);
  return fread(buf, 1, n, file_);
}

size_t VosFile::write(const uint8_t* buf, size_t n) {
  if (!file_ || !buf || !n) return 0;
  drvActAdd(ACT_FS_WRITE);
  return fwrite(buf, 1, n, file_);
}

size_t VosFile::print(const char* text) {
  return text ? write((const uint8_t*)text, strlen(text)) : 0;
}

size_t VosFile::print(char c) {
  return write((const uint8_t*)&c, 1);
}

// Apre un file o una cartella. In lettura: se il percorso e una cartella si apre come cartella.
// In scrittura ("w", "a"): sempre un file, la cartella superiore deve esistere (come prima).
VosFile VosFile::openPath(const char* vfsPath, const char* fsPath, const char* mode) {
  VosFile f;
  if (!vfsPath || !fsPath || fsPath[0] != '/' || !mode) return f;
  struct stat st;
  bool isDir = mode[0] == 'r' && stat(vfsPath, &st) == 0 && S_ISDIR(st.st_mode);
  if (isDir) f.dir_ = opendir(vfsPath);
  else f.file_ = fopen(vfsPath, mode);
  if (!f.file_ && !f.dir_) return f;
  if (f.file_) setvbuf(f.file_, nullptr, _IOFBF, FILE_BUF_BYTES);
  f.isDir_ = isDir;
  f.vfsPath_ = strdup(vfsPath);
  if (!f.vfsPath_) f.close();
  return f;
}

VosFile VosFile::openNextFile() {
  VosFile e;
  if (!dir_ || !vfsPath_) return e;
  struct dirent* de;
  while ((de = readdir(dir_)) != nullptr) {
    if (de->d_type != DT_REG && de->d_type != DT_DIR) continue;        // come Arduino: solo file e cartelle
    size_t pl = strlen(vfsPath_), nl = strlen(de->d_name);
    char* p = (char*)malloc(pl + nl + 2);
    if (!p) return e;
    bool sep = pl > 0 && vfsPath_[pl - 1] != '/';
    snprintf(p, pl + nl + 2, "%s%s%s", vfsPath_, sep ? "/" : "", de->d_name);
    e.vfsPath_ = p;
    e.entry_ = true;
    e.isDir_ = de->d_type == DT_DIR;
    return e;
  }
  return e;
}
