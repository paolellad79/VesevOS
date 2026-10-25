// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_shell_ask.h
// Domande della shell: la conferma (y/n) delle azioni che non si annullano e la richiesta di una password SENZA scriverla nella riga.
//
// Perche: una password scritta nella riga di comando finisce nella cronologia del browser, sullo schermo e negli appunti.
// Quindi "passwd <utente>", "user add <nome> <ruolo>" e "wifi set <rete>" (senza password) la CHIEDONO:
//  - seriale: la domanda resta in sospeso, la riga successiva e la risposta, senza eco; non passa dalla cronologia (la seriale non ne ha);
//  - pagina: il comando risponde "serve la password" (intestazione X-VOS-Ask) e la pagina rimanda lo stesso comando con la password
//    in un campo a parte (pw), mai dentro la riga (vedi shellExecWeb in vos_shell.h).
// La vecchia sintassi con la password in riga resta per non rompere script gia scritti, ma avvisa e non viene mai salvata.
#pragma once
#include <stddef.h>
#include <stdint.h>
#include "../drivers/vos_drv_print.h"
#include "vos_shell_secret.h"

namespace shx {

// ---- conferma di un'azione che non si annulla ----
bool askConfirmed(const char* line);                                      // la riga finisce con la parola "yes"?
void askConfirm(OutSink& o, const char* line, const char* question);      // chiede; se si risponde y/yes/s/si riesegue la riga con " yes" in fondo

// ---- password ----
const char* askSecretGiven(OutSink& o);                                   // la password arrivata dal campo a parte della pagina (nullptr se non c'e)
void askSecret(OutSink& o, SecretFor what, const char* name, int role, bool twice);   // la chiede (twice: due volte, la seconda per controllo)

// ---- seriale: una domanda in sospeso ----
bool askActive();                                                         // la prossima riga e la risposta a una domanda
bool askIsSecret();                                                       // ... una password: niente eco
typedef void (*AskRunFn)(const char* line, OutSink& o, int role, uint32_t sess);   // esegue una riga di shell (la passa vos_shell.cpp: la conferma riesegue il comando)
void askLine(OutSink& o, char* answer, AskRunFn run);   // la risposta (la riga viene cancellata dalla memoria); run riesegue il comando confermato
void askCancel(OutSink& o, bool timeout);                                 // Ctrl+C o troppo tempo: si lascia perdere
bool askExpired(uint32_t nowMs);                                          // la domanda e in sospeso da troppo tempo?
void askClear();                                                          // la sessione finisce (logout): si butta la domanda, senza messaggi, e si cancellano i buffer

// ---- pagina: stato da leggere dopo il comando ----
void askWebBegin(const char* secret);                                     // prima del comando: la password del campo a parte (nullptr = nessuna)
void askWebEnd(char* kind, size_t cap);                                   // dopo: "confirm", "secret", "secret2" oppure "" se il comando non chiede niente
}  // namespace shx
