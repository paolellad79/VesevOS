// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_files.h
// Gestione file e cartelle sulla memoria interna (LittleFS).
// Il file di configurazione (con le password) e protetto: non si vede e non si tocca.
#pragma once
#include <Arduino.h>

String fsClean(const String& path);              // percorso pulito, "" se non valido
bool   fsProtected(const String& cleanPath);     // file di sistema nascosti
String fsListJson(const String& path);           // {"ok":true,"path":"/","list":[{n,d,s}]}
bool   fsMkdir(const String& path, String& err);
bool   fsRemove(const String& path, String& err);   // anche cartelle (con contenuto)
bool   fsRename(const String& from, const String& to, String& err);
bool   fsCopy(const String& from, const String& to, String& err);
bool   fsWriteText(const String& path, const String& text, String& err);
bool   fsReadText(const String& path, String& out, String& err);   // max 8000 caratteri
