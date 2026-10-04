// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_led.h
// LED RGB WS2812 su GPIO48 (SuperMini).
#pragma once
#include <Arduino.h>
#include "vos_common.h"

void ledInit();                       // avvia il task LED
void ledShutdown();                   // ferma il task e spegne il LED (prima del sonno)
void ledSetNetState(NetState s);      // colore = stato rete
void ledApplyConfig();                // rilegge cfg (modo/colore/luminosita/pin)
void ledSetFault(bool on);            // rosso lampeggiante (errore grave)
void ledIdentify(uint32_t ms);        // arcobaleno per ms millisecondi ("trova questa scheda")
void ledSetSetup(bool on);            // arcobaleno lento finche la prima configurazione non e finita
void ledSetAlarm(int level);          // 0 nessuno, 1 giallo (lampo arancione), 2 rosso (lampo rosso) - solo in modo stato
void ledSetHold(int stage);           // tasto BOOT tenuto: 0 no, 1 >=2 s (azzurro), 2 >=8 s (giallo), 3 >=20 s (rosso)
