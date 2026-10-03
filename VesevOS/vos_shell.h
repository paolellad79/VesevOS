// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// VesevOS - vos_shell.h
// Shell comandi condivisa tra seriale e pagina web.
#pragma once
#include <Arduino.h>

// Esegue una riga di comando e scrive il risultato su 'out'.
// authed = l'utente ha gia fatto login (per la seriale vedi serialAuthed()).
void shellExec(const String& line, Print& out, bool authed);

// Gestione shell seriale (da chiamare spesso dal loop o da un task)
void shellSerialPoll();
