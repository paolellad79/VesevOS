// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_ble.h
// Bluetooth (BLE) per telefoni e applicazioni. Spento di fabbrica: si accende da pagina, shell ("ble on [minuti]") o app (API),
// e resta acceso finche non lo si spegne; il limite di tempo e facoltativo (bleSetLimit prima di bleStart).
// Accoppiamento sicuro con codice di 6 cifre mostrato sulla seriale e nella pagina (collegamento cifrato).
// Comandi accettati (testo): wifi <rete>|<password>, country <XX>, name <nome>, mesh <ruolo> <chiave>, status, done.
// Con VOS_WITH_BLE = 0 (vos_common.h) il modulo non c'e e la pagina lo nasconde.
#pragma once
#include <Arduino.h>
#include "../core/vos_common.h"

#if VOS_WITH_BLE
bool   bleStart(String& err);     // acceso (senza limite, o per i minuti di bleSetLimit)
void   bleSetLimit(uint32_t minutes);        // minuti per la prossima accensione (0 = nessun limite, massimo 1440)
bool   bleLimited();                         // true se c'e un tempo che scade
uint32_t bleTotalSec();                      // durata totale del limite in corso (0 = nessun limite)
void   bleStop();
bool   bleRunning();
uint32_t bleLeftSec();                       // secondi rimasti (0 = spento)
void   bleServiceInit();          // iscrive il Bluetooth nel registro dei servizi (vos_service)
void   bleTick();                 // dal ciclo principale: esegue i comandi arrivati, spegne allo scadere
String bleJson();
String bleText();
#else
// Package spento (VOS_WITH_BLE = 0): funzioni vuote, il resto del codice non cambia.
inline bool   bleStart(String& err) { err = "Bluetooth non presente in questo firmware"; return false; }
inline void   bleStop() {}
inline bool   bleRunning() { return false; }
inline void   bleSetLimit(uint32_t) {}
inline bool   bleLimited() { return false; }
inline uint32_t bleTotalSec() { return 0; }
inline uint32_t bleLeftSec() { return 0; }
inline void   bleServiceInit() {}
inline void   bleTick() {}
inline String bleJson() { return "{\"have\":false,\"on\":false}"; }
inline String bleText() { return "Bluetooth: non presente in questo firmware"; }
#endif
