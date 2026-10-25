// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_shell_prompt.cpp
// Compone il prompt della shell (vedi vos_shell_prompt.h). E l'unico punto che lo compone: seriale e pagina usano questo.
#include "vos_shell_prompt.h"
#include "vos_shell_sess.h"
#include "../core/vos_common.h"
#include "../core/vos_config.h"
#include <stdio.h>
#include <string.h>

namespace {

const size_t DIR_SHOW_MAX = 40;   // caratteri di cartella mostrati; oltre, si tiene la coda

// Cartella da mostrare: "~" per la radice, altrimenti il percorso, accorciato a sinistra se troppo lungo.
void dirText(const char* cwd, char* out, size_t cap) {
  if (!cwd || !*cwd || !strcmp(cwd, "/")) { snprintf(out, cap, "~"); return; }
  size_t n = strlen(cwd);
  if (n <= DIR_SHOW_MAX) { snprintf(out, cap, "%.*s", (int)DIR_SHOW_MAX, cwd); return; }
  const char* tail = cwd + (n - (DIR_SHOW_MAX - 3));
  const char* slash = strchr(tail, '/');                 // si taglia su un '/', cosi non resta mezzo nome
  snprintf(out, cap, "...%s", slash ? slash : tail);
}

// Tiene solo i caratteri ASCII stampabili: niente a capo, niente codici di controllo.
void asciiOnly(char* s) {
  for (; *s; s++) if ((unsigned char)*s < 0x20 || (unsigned char)*s > 0x7e) *s = '?';
}

}  // namespace

void shellPrompt(uint32_t sess, const char* user, int role, char* out, size_t cap) {
  if (!out || !cap) return;
  char host[40];
  snprintf(host, sizeof host, "%.39s", cfg.hostname.c_str());
  if (!host[0]) snprintf(host, sizeof host, "vesevos");
  if (!user || !*user) {                                   // nessun utente: solo il nome della scheda
    snprintf(out, cap, "%s> ", host);
    asciiOnly(out);
    return;
  }
  ShellSess ss;
  shellSessLoad(sess, ss);                                 // SESS_NONE o sessione scaduta: radice
  char dir[48], name[24];
  dirText(ss.cwd, dir, sizeof dir);
  snprintf(name, sizeof name, "%s", user);
  snprintf(out, cap, "%s@%s:%s%c ", name, host, dir, role == ROLE_ADMIN ? '#' : '$');
  asciiOnly(out);
}
