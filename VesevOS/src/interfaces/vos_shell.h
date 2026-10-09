// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_shell.h
// Shell comandi condivisa tra seriale e pagina web.
#pragma once
#include <Arduino.h>

// Esegue una riga di comando e scrive il risultato su 'out'.
// role = ruolo di chi scrive: -1 non entrato, 0 Ospite, 1 Operatore, 2 Amministratore (la seriale sbloccata vale 2).
// L'Operatore ha solo i comandi di uso (stato, LED, pin, regole, messaggi); l'Ospite nessuno.
void shellExec(const String& line, Print& out, int role);
// Schermata di benvenuto sulla seriale (completa con la password dell'hotspot solo se full = true)
void shellWelcome(Print& out, bool full);

// Gestione shell seriale (da chiamare spesso dal loop o da un task)
void shellSerialPoll();
void shellSerialShowPass();   // tasto BOOT 8 s: rimostra la schermata con la password dell'hotspot
