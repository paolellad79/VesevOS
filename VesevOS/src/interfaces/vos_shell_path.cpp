// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_shell_path.cpp
// Risoluzione dei percorsi della shell (vedi vos_shell_path.h). Nessuna allocazione: due buffer piccoli sullo stack.
#include "vos_shell_path.h"
#include "../core/vos_files.h"
#include <string.h>

namespace {

// Aggiunge a path (gia normalizzato, "" = radice, mai barra finale) i pezzi di s separati da '/'.
// Falso se il risultato supera FS_PATH_MAX caratteri.
bool feed(char* path, size_t& n, const char* s) {
  while (*s) {
    while (*s == '/') s++;
    const char* b = s;
    while (*s && *s != '/') s++;
    size_t len = (size_t)(s - b);
    if (len == 0 || (len == 1 && b[0] == '.')) continue;          // "//" e "./": niente
    if (len == 2 && b[0] == '.' && b[1] == '.') {                 // "../": su di un livello, ma mai oltre la radice
      while (n > 0 && path[n - 1] != '/') n--;
      if (n > 0) n--;                                             // toglie anche la barra davanti al nome
      path[n] = 0;
      continue;
    }
    if (n + 1 + len > FS_PATH_MAX) return false;
    path[n++] = '/';
    memcpy(path + n, b, len);
    n += len;
    path[n] = 0;
  }
  return true;
}

}  // namespace

bool shellResolvePath(const char* cwd, const char* arg, char* out, size_t cap) {
  if (!out || cap < FS_PATH_MAX + 1) return false;
  if (!arg) arg = "";
  for (const char* p = arg; *p; p++) {                            // gli stessi caratteri che fsCleanTo non ammette, prima di toccare niente
    unsigned char c = (unsigned char)*p;
    if (c < 32 || c >= 127 || c == '\\') return false;
  }
  char path[FS_PATH_MAX + 2];
  size_t n = 0;
  path[0] = 0;
  bool home = (arg[0] == '~' && (arg[1] == 0 || arg[1] == '/'));  // "~" e "~/..." = radice; "~nome" e un nome come un altro
  if (home) arg++;
  else if (arg[0] != '/' && cwd && !feed(path, n, cwd)) return false;
  if (!feed(path, n, arg)) return false;
  if (n == 0) { path[0] = '/'; path[1] = 0; }
  return fsCleanTo(path, out, cap);                               // l'ultima parola resta alle regole di sempre
}
