// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_stats.h
// Statistiche d'uso: contatori anonimi, SOLO in RAM, SPENTE di fabbrica. Niente MAC, niente indirizzi IP, niente dati personali;
// nulla esce dalla scheda. Si azzerano al riavvio (o con il pulsante Azzera).
#pragma once
#include <Arduino.h>

enum StatEvent : uint8_t { ST_BLE_START = 0, ST_BLE_CONN, ST_BOOT_SHORT, ST_BOOT_2S, ST_BOOT_8S, ST_BOOT_20S };
#include "vos_common.h"

#if VOS_WITH_STATS
void   statsTick();                       // dal ciclo principale (ogni 10 s se acceso)
void   statNote(StatEvent e);             // un evento (ignorato se spento)
void   statsReset();
String statsJson();
String statsCsv();
String statsText();
#else
inline void   statsTick() {}
inline void   statNote(StatEvent) {}
inline void   statsReset() {}
inline String statsJson() { return "{\"present\":false}"; }
inline String statsCsv() { return ""; }
inline String statsText() { return "Statistiche: non presenti in questo firmware"; }
#endif
