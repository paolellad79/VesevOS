// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_led.h
// LED RGB WS2812 su GPIO48 (SuperMini).
#pragma once
#include <Arduino.h>
#include "vos_common.h"

void ledInit();                       // avvia il task LED
void ledSetNetState(NetState s);      // colore = stato rete
void ledApplyConfig();                // rilegge cfg (modo/colore/luminosita/pin)
void ledSetFault(bool on);            // rosso lampeggiante (errore grave)
bool led2PinAllowed(int pin);        // pin ammesso per il LED aggiuntivo
