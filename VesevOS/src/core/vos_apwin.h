// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_apwin.h
// Finestra del hotspot aperto (senza password): si apre con una pressione breve del tasto BOOT, solo prima della
// configurazione, e dura 10 minuti. Ora e una vista di vos_primo_avvio (la macchina a stati sta li).
#pragma once
#include <stdint.h>

void apWinOpen();          // pressione del tasto: apre (o rinnova) la finestra
bool apWinIsOpen();        // la finestra e' aperta adesso?
void apWinClose();         // fine della guida: la scheda torna normale
void apWinAttivita();       // richiesta con accesso durante la guida: rinnova i 10 minuti
