// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_apwin.h
// Finestra del hotspot senza password: si apre con una pressione del tasto BOOT,
// solo prima della configurazione (o dopo un reset). Dura 10 minuti, poi il hotspot torna con la password.
#pragma once
#include <stdint.h>

void apWinOpen();          // apre la finestra (10 minuti)
bool apWinIsOpen();        // la finestra e' aperta adesso?
void apWinClose();         // chiude la finestra (fine configurazione)
bool apWinTick();          // da chiamare spesso: true se la finestra e' scaduta da sola (chi chiama riapplica l'hotspot)
