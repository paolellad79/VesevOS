// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_drv_file.h
// STRATO 1 (driver): il tipo VosFile, un file o una cartella aperti sul file system interno.
// E' nostro (sta sopra POSIX/VFS: fopen, opendir, stat): non dipende dal tipo File di Arduino.
// Ha solo i metodi che il firmware usa davvero, con lo stesso significato di Arduino:
//   - un file: read, write, print, size, available, seek, position, close
//   - una cartella: isDirectory, openNextFile (le voci sono leggere: nome, tipo e dimensione, NON aprono il file)
// Non si copia (possiede il file aperto); si sposta. Chiude da solo quando esce dal blocco.
#pragma once
#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include <dirent.h>

class VosFile {
 public:
  VosFile() = default;
  VosFile(VosFile&& o) noexcept { take(o); }
  VosFile& operator=(VosFile&& o) noexcept { if (this != &o) { close(); take(o); } return *this; }
  VosFile(const VosFile&) = delete;
  VosFile& operator=(const VosFile&) = delete;
  ~VosFile() { close(); }

  explicit operator bool() const { return file_ || dir_ || entry_; }   // aperto (file, cartella o voce di una cartella)
  bool isDirectory() const { return isDir_; }
  const char* name() const;               // solo il nome, senza le cartelle
  size_t size() const;                    // byte del file (0 per una cartella)
  int available() const;                  // byte ancora da leggere
  bool seek(size_t pos);                  // porta la lettura/scrittura al byte pos dall'inizio (solo file); false se non riesce
  size_t position() const;                // byte dall'inizio dove si e arrivati (0 se non e un file)
  void close();

  int    read();                          // un byte, -1 a fine file
  size_t read(uint8_t* buf, size_t n);
  size_t write(const uint8_t* buf, size_t n);
  size_t print(const char* text);
  size_t print(char c);

  VosFile openNextFile();                 // prossima voce di una cartella (vuota quando sono finite)

  // Usate da drvFsOpen: aprono il percorso VFS completo (es. "/littlefs/dati.txt"); fsPath e' il percorso dentro il file system ("/dati.txt").
  static VosFile openPath(const char* vfsPath, const char* fsPath, const char* mode);

 private:
  FILE* file_ = nullptr;
  DIR*  dir_ = nullptr;
  char* vfsPath_ = nullptr;               // percorso VFS completo (serve per size() delle voci e per le voci di una cartella)
  bool  entry_ = false;                   // voce di una cartella: nessun file aperto
  bool  isDir_ = false;
  void take(VosFile& o);
};
