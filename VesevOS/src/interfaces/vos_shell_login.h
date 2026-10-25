// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_shell_login.h
// Il login della seriale, come in una shell classica:   <hostname> login: <utente>   Password:   Codice MFA: (solo se l'utente ha l'MFA)
//
// - La password e il codice si scrivono senza eco; i buffer si cancellano dopo l'uso.
// - Un errore (utente, password o codice) da' sempre lo stesso messaggio, dopo la stessa attesa: non si capisce se l'utente esiste.
// - Gli errori contano nel blocco CONDIVISO col web (authFailIp con l'origine AUTH_IP_SERIAL): stesso tetto, stessa durata.
// - Ogni tentativo va nell'audit e nel registro (utente ed esito, mai la password o il codice).
// - Ogni passo ha 60 s di tempo; poi si torna a "login:".
// Niente blocchi: l'attesa dopo un errore e un'ora di scadenza, non una pausa (il loop della seriale resta libero).
#pragma once
#include <stddef.h>
#include <stdint.h>
#include "../drivers/vos_drv_print.h"

namespace shx {
void loginPrompt(OutSink& o);                     // scrive la richiesta del passo attuale ("host login: ", "Password: " o "Codice MFA: ")
bool loginEchoes();                               // il passo attuale mostra quello che si scrive? (solo il nome utente)
bool loginBusy();                                 // attesa dopo un errore: i tasti si buttano
// Una riga arrivata durante il login. La riga viene cancellata dalla memoria.
// Ritorna true se l'accesso e riuscito (la sessione seriale e aperta: chi chiama scrive il prompt della shell);
// altrimenti ha gia scritto da se la richiesta del passo dopo.
bool loginLine(OutSink& o, char* line);
// Da chiamare a ogni giro: chiude l'attesa dopo un errore e scade i passi. Ritorna true se ha cambiato passo (chi chiama svuota la riga in digitazione).
bool loginTick(OutSink& o, uint32_t nowMs);
void loginReset();                                // torna al passo del nome (dopo un logout)
void wipeBytes(void* p, size_t n);                // cancella davvero (il compilatore non puo togliere la scrittura)
}  // namespace shx
