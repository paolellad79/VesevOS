// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_ble_int.h: stato condiviso tra vos_ble.cpp (ciclo di vita) e vos_ble_app.cpp (funzioni dei programmi). Non usare altrove.
#pragma once
#include "vos_ble_types.h"
#include "../core/vos_lock.h"

#if VOS_WITH_BLE
namespace vosble {
enum Phase : uint8_t { IDLE = 0, WANT = 1, UP = 2 };   // WANT = richiesto, lo avvia bleTick
extern VosMutex g_mx;                                   // tutte le funzioni pubbliche lo prendono (task Berry, ciclo principale)
extern Phase    g_phase;
extern char     g_owner[BLE_OWNER_MAX + 1];             // "" = libero
extern char     g_name[BLE_NAME_MAX + 1];               // nome da annunciare
extern BleServiceDef g_def;                             // servizio scelto dal programma
extern bool     g_adv;                                  // annuncio richiesto
extern uint32_t g_limMin;                               // minuti chiesti (0 = finche il programma gira)
extern bool     g_endReq;                               // il programma e finito: spegnere
extern char     g_err[12];                              // ultimo motivo di spegnimento anomalo ("" = nessuno)
extern uint32_t g_stopAt;
extern bool     g_conn;                                 // un telefono e collegato
void onLinkEvent(bool up);                              // dal driver: collegato / scollegato

bool claim(const char* owner, ErrBuf& err);             // prende (o conferma) il possesso; false = occupato da un altro
void resetConfig();                                     // nome, servizio, annuncio, minuti ai valori di partenza
BleDrvEvents* driverEvents();                           // callback verso la coda degli eventi
void queueClear();
}
#endif
