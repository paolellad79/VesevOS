// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_shell_prompt.h
// Il prompt della shell, uguale a quello di bash: utente@hostname:cartella$
#pragma once
#include <stddef.h>
#include <stdint.h>

#define SHELL_PROMPT_MAX 128   // dimensione consigliata del buffer (il testo e sempre chiuso con 0 e mai piu lungo)

// Scrive in out il prompt per la sessione sess (SESS_* in vos_shell_sess.h).
//   user = nome con cui si e entrati (nullptr o vuoto = nessuno);  role = ruolo (ROLE_ADMIN = '#', gli altri '$').
// Formato:
//   con utente:   utente@hostname:cartella$     ('#' al posto di '$' per l'amministratore, poi uno spazio)
//   senza utente: hostname>                      (prima di entrare, o primo avvio senza password; poi uno spazio)
// hostname = quello di adesso (cfg.hostname), letto a ogni chiamata. cartella = la cartella corrente della sessione;
// "/" si scrive "~" (la casa di tutti e la radice). Un percorso lungo si accorcia a sinistra: ".../ultimi/livelli".
// Solo caratteri ASCII stampabili (gli altri diventano '?'): il prompt non puo portare a capo ne comandi.
void shellPrompt(uint32_t sess, const char* user, int role, char* out, size_t cap);
