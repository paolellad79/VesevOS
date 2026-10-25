// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_shell.h
// Shell comandi condivisa tra seriale e pagina web.
#pragma once
#include <stddef.h>
#include <stdint.h>
#include "../drivers/vos_drv_print.h"

// Esegue una riga di comando e scrive il risultato su 'out'.
// role = ruolo di chi scrive: -1 non entrato, 0 Ospite, 1 Operatore, 2 Amministratore (la seriale sbloccata vale 2).
// L'Operatore ha solo i comandi di uso (stato, LED, pin, regole, messaggi); l'Ospite nessuno.
void shellExec(const char* line, OutSink& out, int role);
// Come shellExec, ma ricorda la cartella corrente (cd, pwd, percorsi relativi) per la sessione sess (SESS_* in vos_shell_sess.h).
// shellExec e SESS_NONE: nessuna memoria, i percorsi partono sempre da "/" (regole, script).
void shellExecSess(const char* line, OutSink& out, int role, uint32_t sess);
// Per la pagina web: come shellExecSess, con due cose in piu.
//  secret: la password scritta nel campo a parte della pagina (nullptr = nessuna). Non sta mai nella riga: cosi non finisce in cronologia.
//  ask:    dopo il comando contiene "confirm" (serve una conferma: rimandare il comando con " yes"), "secret" o "secret2" (serve una
//          password, la seconda volta da ripetere) oppure "" se il comando non chiede niente. askCap = dimensione di ask.
//  clientIp: l'IP di chi scrive, in ordine "a<<24|b<<16|c<<8|d" (0 = non noto). Lo usa solo vesevos-update file, che lega il gettone a quel computer.
void shellExecWeb(const char* line, const char* secret, OutSink& out, int role, uint32_t sess, char* ask, size_t askCap, uint32_t clientIp = 0);
// Schermata di benvenuto sulla seriale (completa con la password dell'hotspot solo se full = true)
void shellWelcome(OutSink& out, bool full);

// Gestione shell seriale (da chiamare spesso dal loop o da un task)
void shellSerialPoll();
void shellSerialWho(OutSink& out);   // riga "seriale" per il comando who (stato e ultimo comando, solo la prima parola)
void shellSerialShowPass();   // tasto BOOT 8 s: rimostra la schermata con la password dell'hotspot
