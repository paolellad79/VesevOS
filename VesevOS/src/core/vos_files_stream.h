// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_files_stream.h
// Scrittura di un file a pezzi, con la stessa sicurezza di fsWriteText (vos_files.h) ma senza il tetto di 32 KB e senza tenere il file in
// memoria: si scrive in "nome.tmp~" e, solo a lavoro finito, lo scambio (rename) sostituisce il file vero in un passo. Se manca la corrente o
// qualcosa va storto a meta, il file vecchio resta intero. Le regole sono quelle di sempre: percorso pulito, file protetti rifiutati,
// al massimo FS_DIR_MAX voci per cartella, spazio libero con un margine.
// Serve a gzip, unzip e tar (interfaces/vos_shell_arch*.cpp, vos_shell_gzip.cpp...).
#pragma once
#include "vos_files.h"
#include "../drivers/vos_drv_file.h"

// Spazio libero della memoria interna, in byte: l'ultimo valore ricordato (vedi drvFsUsed: puo avere fino a 60 s di ritardo).
size_t fsFreeBytes();

// Crea la cartella e tutte quelle che mancano sopra (come "mkdir -p"). Ogni cartella passa da fsMkdir: tetto di voci e file protetti compresi.
// Una cartella che c'e gia va bene; un file con quel nome no. cleanDir: percorso gia pulito.
bool fsMkdirs(const char* cleanDir, ErrBuf& err);

class FsAtomicWriter {
 public:
  FsAtomicWriter() = default;
  ~FsAtomicWriter() { abort(); }
  FsAtomicWriter(const FsAtomicWriter&) = delete;
  FsAtomicWriter& operator=(const FsAtomicWriter&) = delete;

  // Apre il temporaneo. path: percorso gia risolto. overwrite = false rifiuta un file che c'e gia. maxBytes: tetto di quanto si scrive
  // (il vero tetto e anche lo spazio libero meno un margine). Da qui al commit()/abort() il blocco dei salvataggi e preso.
  bool begin(const char* path, bool overwrite, size_t maxBytes, ErrBuf& err);
  bool write(const uint8_t* data, size_t n);     // false se supera il tetto o non c'e spazio (vedi full())
  // Riscrive n byte gia scritti, dal byte pos (serve a zip per mettere CRC e dimensioni nell'intestazione dopo i dati). Poi la scrittura
  // riprende dalla fine; non cambia written(). false se pos+n va oltre quanto scritto o se la scrittura fallisce.
  bool patch(size_t pos, const uint8_t* data, size_t n);
  bool commit(ErrBuf& err);                      // chiude e scambia; dopo, l'oggetto e vuoto
  void abort();                                  // butta il temporaneo (si puo chiamare sempre; lo fa anche il distruttore)
  size_t written() const { return n_; }
  bool   full() const { return full_; }          // l'ultima write e fallita per il tetto o per lo spazio
  bool   isOpen() const { return open_; }

 private:
  VosFile f_;
  char    tmp_[FS_PATH_MAX + 8] = "";
  char    dst_[FS_PATH_MAX + 1] = "";
  size_t  max_ = 0;
  size_t  n_ = 0;
  bool    open_ = false;
  bool    locked_ = false;
  bool    full_ = false;
};
