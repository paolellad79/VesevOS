// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_common.h
// Definizioni comuni a tutti i moduli. Solo caratteri ASCII.
#pragma once
#include <Arduino.h>
#include "../boards/vos_board.h"   // numeri della scheda (pin, memoria, funzioni presenti)

#define VOS_NAME     "VesevOS"
// 1 = i blocchi grandi (buffer TLS) vanno in PSRAM; se la pagina diventa instabile mettere 0
#ifndef VOS_PSRAM_MALLOC
#define VOS_PSRAM_MALLOC 1
#endif
#define VOS_VERSION "1.7.65a"
#define VOS_API_VERSION 1                       // cambia solo per rotture dell'API (regole API 15)
#define VOS_GITHUB   "https://github.com/paolellad79/VesevOS"

#include "../boards/vos_packages.h"   // interruttori dei package (VOS_WITH_*)

// Ruoli degli utenti (multiutenza)
enum UserRole : uint8_t { ROLE_GUEST = 0, ROLE_OPER = 1, ROLE_ADMIN = 2 };
#define VOS_MAX_USERS 8

// Stato della rete (usato anche per il colore del LED)
enum NetState : uint8_t {
  NET_BOOT = 0,
  NET_AP,
  NET_CLIENT_TRY,
  NET_CLIENT_OK,
  NET_AIR          // modalita aereo: radio spenta
};

// Modalita del LED
enum LedMode : uint8_t {
  LED_STATE = 0,   // colore = stato del sistema
  LED_HEARTBEAT,   // battito legato al carico CPU
  LED_FIXED,       // colore fisso
  LED_OFF
};
