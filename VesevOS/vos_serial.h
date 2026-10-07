// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_serial.h
// Impostazioni della seriale (cavo USB): velocita, a-capo, eco, comandi, log, benvenuto, attesa di scrittura.
#pragma once
#include <Arduino.h>

void    serialApply();                                   // applica le impostazioni (all'avvio e dopo ogni modifica)
Print&  serOut();                                        // uscita sulla seriale con l'a-capo scelto
bool    serIsOut(Print& o);                              // true se o e la seriale
void    serLogLine(const char* line);                    // riga di log sulla seriale (rispetta "scrivi il log")
bool    serialSet(const String& key, const String& val, String& err);   // key: baud eol echo input log banner tx
String  serialText();                                    // riassunto per la shell
String  serialJson();                                    // per la pagina
bool    serialKeep();                                    // conferma la nuova velocita
void    serialTick();                                    // ogni giro di loop: se la velocita nuova non e confermata torna la vecchia
