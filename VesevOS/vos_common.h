// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_common.h
// Definizioni comuni a tutti i moduli. Solo caratteri ASCII.
#pragma once
#include <Arduino.h>

#define VOS_NAME     "VesevOS"
// 1 = i blocchi grandi (buffer TLS) vanno in PSRAM; se la pagina diventa instabile mettere 0
#ifndef VOS_PSRAM_MALLOC
#define VOS_PSRAM_MALLOC 1
#endif
#define VOS_VERSION  "1.7.11"
#define VOS_GITHUB   "https://github.com/paolellad79/VesevOS"
#define VOS_BOARD    "ESP32-S3 SuperMini"   // scheda (tabella dei piedini nella pagina)

// PACKAGE (moduli opzionali): 1 = dentro il firmware, 0 = tolti (meno flash e meno RAM).
// Per toglierne uno basta mettere 0 e ricompilare: i servizi e le voci in pagina/shell spariscono da soli.
// Si possono anche cambiare dal menu Arduino con "build_opt.h" oppure qui sotto.
#ifndef VOS_WITH_BLE
#define VOS_WITH_BLE 1      // Bluetooth per la prima configurazione (NimBLE)
#endif
#ifndef VOS_WITH_MQTT
#define VOS_WITH_MQTT 1     // client MQTT (broker, Home Assistant)
#endif
#ifndef VOS_WITH_MESH
#define VOS_WITH_MESH 1     // ESP-NOW tra schede
#endif
#ifndef VOS_WITH_MFA
#define VOS_WITH_MFA 1      // codice a due passaggi (TOTP) al login
#endif
#ifndef VOS_WITH_STATS
#define VOS_WITH_STATS 1    // statistiche d'uso (contatori locali)
#endif

// Ruoli degli utenti (multiutenza)
enum UserRole : uint8_t { ROLE_GUEST = 0, ROLE_OPER = 1, ROLE_ADMIN = 2 };
#define VOS_MAX_USERS 8

// Pin predefiniti per ESP32-S3 SuperMini
#define VOS_PIN_LED_RGB   48   // LED WS2812 integrato
#define VOS_PIN_BOOT       0   // pulsante BOOT

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
