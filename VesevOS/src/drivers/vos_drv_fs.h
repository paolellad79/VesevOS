// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_drv_fs.h
// STRATO 1 (driver): unico file che parla con il file system interno (LittleFS, componente esp_littlefs: la memoria interna della scheda).
// Gli altri file usano queste funzioni. Il tipo dei file aperti e VosFile (vos_drv_file.h), nostro, sopra POSIX/VFS.
// Partizione "spiffs", montata su /littlefs: stessa partizione e stesso formato di prima (i file gia salvati si leggono).
// Non decide cosa scrivere ne dove: lo decidono i servizi (config, diario, lingue, file...).
#pragma once
#include <stddef.h>
#include <stdint.h>
#include "vos_drv_file.h"

bool   drvFsBegin(bool formatOnFail);
VosFile drvFsOpen(const char* path, const char* mode = "r");        // "r" lettura, "w" scrittura (crea/azzera), "a" aggiunge
bool   drvFsExists(const char* path);
bool   drvFsRemove(const char* path);
bool   drvFsRename(const char* from, const char* to);
bool   drvFsMkdir(const char* path);
bool   drvFsRmdir(const char* path);
size_t drvFsUsed();                                          // spazio usato: l'ultimo valore ricordato (0 prima del primo calcolo), subito e senza scorrere il file system
bool   drvFsUsedDue(uint32_t nowSec);                        // e ora di ricalcolarlo? (mai nei primi 20 s; dopo scritture non prima di 60 s dall'ultimo; comunque ogni 10 minuti)
void   drvFsUsedRefresh(uint32_t nowSec);                    // lo ricalcola (lento con tante voci): da un solo task di fondo, mai dal web server
size_t drvFsTotal();
bool   drvFsVfsPath(const char* path, char* out, size_t outSize);   // percorso VFS completo ("/dati.txt" -> "/littlefs/dati.txt"), per chi serve file al web server
