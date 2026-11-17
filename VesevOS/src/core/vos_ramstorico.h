// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_ramstorico.h
// Anello di 40 annotazioni della RAM (traccia dell'avvio e `ram audit`). Nessun heap, nessuna dipendenza dal registro o dalle traduzioni:
// cosi lo possono usare sia vos_ram sia vos_log senza chiudere un ciclo tra moduli.
#pragma once
#include <stdint.h>

#define RAM_NOTES 40
struct RamNote { uint32_t sec; char label[10]; uint32_t free_, big, ps; };

void    ramNote(const char* label);          // annota la RAM di adesso con un nome corto
uint8_t ramNoteCount();                      // quante voci valide (max RAM_NOTES)
bool    ramNoteGet(uint8_t i, RamNote& out); // voce i-esima, dalla piu vecchia (0) alla piu nuova
