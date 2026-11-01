// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_paths.cpp (vedi vos_paths.h). Nessuna allocazione: buffer piccoli sullo stack.
#include "vos_paths.h"
#include "vos_layout.h"
#include "../boards/vos_packages.h"
#include <string.h>
#include <strings.h>
#include <ctype.h>

namespace {

const char MEDIA_DIR[] = "/media";
const char SD_SHORT[] = "/sd";

// Il percorso comincia con prefix e subito dopo finisce o c'e una barra? (cosi "/sdx" non e "/sd")
bool hasPrefixDir(const char* path, const char* prefix) {
  size_t n = strlen(prefix);
  return strncmp(path, prefix, n) == 0 && (path[n] == 0 || path[n] == '/');
}

}  // namespace

bool pathSdShortcut(char* path, size_t cap, size_t maxLen) {
  if (!hasPrefixDir(path, SD_SHORT)) return true;
  const size_t from = sizeof(SD_SHORT) - 1;
  const size_t to = sizeof(VOS_SD_MOUNT_POINT) - 1;
  size_t total = strlen(path) - from + to;                        // lunghezza dopo la traduzione
  if (total > maxLen || total + 1 > cap) return false;
  memmove(path + to, path + from, strlen(path) - from + 1);       // il resto, zero compreso
  memcpy(path, VOS_SD_MOUNT_POINT, to);
  return true;
}

bool pathLayoutShortcut(char* path, size_t cap, size_t maxLen) {
  bool useNew = layoutState() >= 2;                               // da quando la copia e finita i percorsi in uso sono i nuovi
  for (int i = 0; i < LAY_COUNT; i++) {
    LayoutItem it = (LayoutItem)i;
    const char* from = useNew ? layoutPathOld(it) : layoutPathNew(it);   // il nome che NON e in uso
    const char* to = useNew ? layoutPathNew(it) : layoutPathOld(it);
    size_t fl = strlen(from), tl = strlen(to);
    bool isDir = i <= LAY_BACKUP;                                 // le voci di file valgono solo per il nome intero
    if (isDir ? !hasPrefixDir(path, from) : strcmp(path, from) != 0) continue;
    size_t total = strlen(path) - fl + tl;
    if (total > maxLen || total + 1 > cap) return false;
    memmove(path + tl, path + fl, strlen(path) - fl + 1);
    memcpy(path, to, tl);
    return true;
  }
  return true;
}

bool pathIsMedia(const char* clean) { return hasPrefixDir(clean, MEDIA_DIR); }

bool pathIsMediaRoot(const char* clean) { return strcmp(clean, MEDIA_DIR) == 0; }

const char* pathSdRel(const char* clean) {
  if (!hasPrefixDir(clean, VOS_SD_MOUNT_POINT)) return nullptr;
  const char* rel = clean + sizeof(VOS_SD_MOUNT_POINT) - 1;
  return *rel ? rel : "/";
}

namespace {

// Lettere e numeri ASCII, piu pochi simboli sicuri.
bool sdAllowedChar(char c) {
  bool asciiAlnum = isalnum((unsigned char)c) && (unsigned char)c < 128;
  bool symbol = c == '_' || c == '-' || c == '.' || c == '/' || c == ' ' || c == '(' || c == ')';
  return asciiAlnum || symbol;
}

// Il carattere in posizione i rende il percorso non valido?
bool sdBadCharAt(const char* p, size_t n, size_t i) {
  char c = p[i];
  char prev = i > 0 ? p[i - 1] : 0;
  char next = i + 1 < n ? p[i + 1] : 0;
  if (!sdAllowedChar(c)) return true;
  if (c == '/' && next == '/') return true;                       // "//"
  if (c == '.' && next == '.') return true;                       // ".."
  if (prev == '/' && c == ' ') return true;                       // nome che comincia con spazio
  return c == '/' && (prev == ' ' || prev == '.');                // nome che finisce con spazio o punto e "/./"
}

}  // namespace

bool pathSdRelOk(const char* rel) {
  const size_t kMax = 100;
  size_t n = rel ? strlen(rel) : 0;
  if (n == 0 || n > kMax || rel[0] != '/') return false;
  for (size_t i = 0; i < n; i++) if (sdBadCharAt(rel, n, i)) return false;
  if (n > 1) {                                                    // niente "/" ne spazio ne punto in fondo (tranne la radice)
    char last = rel[n - 1];
    if (last == '/' || last == ' ' || last == '.') return false;
  }
  return true;
}

static bool systemName(const char* p) {
  return !strncmp(p, "/etc/vesevos.conf", 17) || !strncmp(p, "/vesevos.conf", 13) || !strncmp(p, "/audit.log", 10) || !strncmp(p, "/crash.txt", 10) ||
         !strncmp(p, "/var/audit.log", 14) || !strncmp(p, "/var/crash.txt", 14);   // configurazione (nuova in /etc e vecchia), registro di audit e rapporto dei crash (posto nuovo in /var e vecchio), anche con .bak .tmp .tmp~: solo dal firmware
}

bool pathIsSystemFile(const char* clean) {
  if (systemName(clean)) return true;
  const char* rel = pathSdRel(clean);
  return rel && systemName(rel);                                  // anche una copia dei nomi di sistema sulla scheda: stessa regola
}

bool pathIsSecret(const char* clean) {
  if (pathIsSystemFile(clean)) return true;
  static const char* const kExt[] = {".pem", ".key", ".crt", ".der", ".p12", ".vbk"};
  size_t n = strlen(clean);
  for (const char* ext : kExt) {
    size_t m = strlen(ext);
    if (n >= m && strcasecmp(clean + n - m, ext) == 0) return true;
  }
  return false;
}
