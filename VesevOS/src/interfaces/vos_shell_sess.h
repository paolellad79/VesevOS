// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_shell_sess.h
// Stato della shell per sessione: la cartella corrente (cd, pwd, percorsi relativi).
// Una tabella fissa di poche sessioni: nessuna memoria che cresce. Una sessione ferma da troppo tempo torna alla radice.
#pragma once
#include <stdint.h>
#include "../core/vos_files.h"

// Chi sta scrivendo. SESS_NONE = nessuna sessione (regole, script): parte sempre da "/" e non ricorda niente.
#define SESS_NONE    0u
#define SESS_SERIAL  1u                  // la seriale USB (una sola)
#define SESS_WEB     2u                  // pagina web: 2 + indice dell'utente (stesso utente = stessa cartella)
#define SESS_MAX     8                   // sessioni ricordate insieme; se sono piene si butta la piu vecchia
#define SESS_IDLE_MS 1800000UL           // 30 minuti senza comandi: la cartella torna "/"

struct ShellSess {
  char cwd[FS_PATH_MAX + 1];             // cartella corrente, sempre un percorso pulito che comincia con '/'
  char prev[FS_PATH_MAX + 1];            // cartella di prima (per "cd -")
  ShellSess() { cwd[0] = '/'; cwd[1] = 0; prev[0] = '/'; prev[1] = 0; }
};

void shellSessLoad(uint32_t id, ShellSess& s);          // copia lo stato della sessione (o quello di partenza)
void shellSessStore(uint32_t id, const ShellSess& s);   // lo risalva (SESS_NONE: non fa niente)
void shellSessDrop(uint32_t id);                        // la sessione finisce (logout)
