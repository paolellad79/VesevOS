// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_drv_fs.h
// STRATO 1 (driver): unico file che usa LittleFS (la memoria interna della scheda). Gli altri file usano queste funzioni.
// Il tipo File e quello di Arduino (stessi metodi: read, write, close, size, isDirectory, openNextFile...).
// Non decide cosa scrivere ne dove: lo decidono i servizi (config, diario, lingue, file...).
#pragma once
#include <Arduino.h>
#include <FS.h>

bool   drvFsBegin(bool formatOnFail);
File   drvFsOpen(const String& path, const char* mode = "r");      // "r" lettura, "w" scrittura (crea/azzera), "a" aggiunge
bool   drvFsExists(const String& path);
bool   drvFsRemove(const String& path);
bool   drvFsRename(const String& from, const String& to);
bool   drvFsMkdir(const String& path);
bool   drvFsRmdir(const String& path);
size_t drvFsUsed();
size_t drvFsTotal();
FS&    drvFsHandle();                                               // il file system stesso, per chi serve file (server web)
