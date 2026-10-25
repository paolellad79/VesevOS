// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_drv_console.h
// STRATO 1 (driver): console seriale sulla USB nativa (USB Serial/JTAG di ESP-IDF, driver `usb_serial_jtag`).
// Il resto del codice non usa Serial: passa da qui. Implementazione in vos_drv_console.cpp.
// Nessun Arduino: la scheda ha solo la USB nativa, quindi non c'e piu la UART (la velocita non conta).
#pragma once
#include "vos_drv_string.h"        // tipi standard (stdint, string.h...) che i chiamanti prima ricevevano da qui

// Installa il driver USB Serial/JTAG (buffer TX 512 byte, RX 256 byte). La velocita non conta; si puo chiamare piu volte.
void drvConBegin(uint32_t baud);
// Attesa massima a ogni scrittura quando il PC e collegato ma non legge (0 = non aspetta, i byte in piu si perdono).
void drvConSetTxTimeoutMs(uint32_t ms);
// Scrive n byte; restituisce quanti ne sono usciti (meno di n = il PC non legge).
// Senza cavo non aspetta mai: i byte restano nel buffer finche c'e posto, poi si buttano e si restituisce n.
// Con il cavo ma il monitor chiuso aspetta al massimo il tempo impostato, poi per 2 secondi non aspetta piu.
size_t drvConWrite(const uint8_t* b, size_t n);
// Byte in arrivo da leggere subito.
int drvConAvailable();
// Legge un byte (-1 se non ce ne sono).
int drvConRead();
// Aspetta (al massimo 50 ms) che quello scritto sia uscito, prima del sonno profondo. Senza PC non aspetta.
void drvConFlush();
// Vero quando un monitor seriale e collegato e legge (cavo presente e ultima scrittura riuscita).
bool drvConIsOpen();
