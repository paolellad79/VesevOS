// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_serial.h
// Impostazioni della seriale (cavo USB): velocita, a-capo, eco, comandi, log, benvenuto, attesa di scrittura.
#pragma once
#include "vos_errbuf.h"
#include "vos_heapbuf.h"
#include "../drivers/vos_drv_print.h"
#include <string>

void    serialApply();                                   // applica le impostazioni (all'avvio e dopo ogni modifica)
OutSink&  serOut();                                        // uscita sulla seriale con l'a-capo scelto
bool    serIsOut(OutSink& o);                              // true se o e la seriale
void    serLogLine(const char* line);                    // riga di log sulla seriale (rispetta "scrivi il log")
bool    serialSet(const char* key, const char* val, ErrBuf& err);   // key: baud eol echo input log banner tx
std::string serialText();                               // riassunto per la shell
#define SERIAL_JSON_MAX 256
bool    serialJson(HeapBuf& out);                                 // per la pagina
bool    serialKeep();                                    // conferma la nuova velocita
void    serialTick();                                    // ogni giro di loop: se la velocita nuova non e confermata torna la vecchia
uint32_t serStallCount();                                // quante volte il PC non ha letto in tempo (scritture buttate via)
