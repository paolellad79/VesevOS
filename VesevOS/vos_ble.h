// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_ble.h
// Bluetooth (BLE) SOLO per configurare una scheda nuova dal telefono. Spento di fabbrica:
// si accende a mano (pagina o shell "ble on") per 10 minuti, poi si spegne da solo.
// Accoppiamento sicuro con codice di 6 cifre mostrato sulla seriale e nella pagina (collegamento cifrato).
// Comandi accettati (testo): wifi <rete>|<password>, country <XX>, name <nome>, mesh <ruolo> <chiave>, status, done.
// Con VOS_WITH_BLE = 0 (vos_common.h) il modulo non c'e e la pagina lo nasconde.
#pragma once
#include <Arduino.h>

bool   bleStart(String& err);     // acceso per 10 minuti
void   bleStop();
bool   bleRunning();
void   bleTick();                 // dal ciclo principale: esegue i comandi arrivati, spegne allo scadere
String bleJson();
String bleText();
