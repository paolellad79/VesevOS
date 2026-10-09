// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_drv_fs.cpp
#include "vos_drv_fs.h"
#include <LittleFS.h>

bool   drvFsBegin(bool formatOnFail) { return LittleFS.begin(formatOnFail); }
File   drvFsOpen(const String& path, const char* mode) { return LittleFS.open(path, mode); }
bool   drvFsExists(const String& path) { return LittleFS.exists(path); }
bool   drvFsRemove(const String& path) { return LittleFS.remove(path); }
bool   drvFsRename(const String& from, const String& to) { return LittleFS.rename(from, to); }
bool   drvFsMkdir(const String& path) { return LittleFS.mkdir(path); }
bool   drvFsRmdir(const String& path) { return LittleFS.rmdir(path); }
size_t drvFsUsed() { return LittleFS.usedBytes(); }
size_t drvFsTotal() { return LittleFS.totalBytes(); }
FS&    drvFsHandle() { return LittleFS; }
