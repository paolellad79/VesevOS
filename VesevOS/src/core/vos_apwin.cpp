// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_apwin.cpp (vedi vos_apwin.h)
#include "vos_apwin.h"
#include "vos_primo_avvio.h"

void apWinOpen()   { primoAvvioPressione(); }
bool apWinIsOpen() { return primoAvvioFinestra(); }
void apWinClose()  { primoAvvioFine(); }
void apWinAttivita() { primoAvvioAttivita(); }
