// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_common.h
// Definizioni comuni a tutti i moduli. Solo caratteri ASCII.
#pragma once
#include <Arduino.h>

#define VOS_NAME     "VesevOS"
#define VOS_VERSION  "1.4.3"
#define VOS_GITHUB   "https://github.com/paolellad79/VesevOS"

// Pin predefiniti per ESP32-S3 SuperMini
#define VOS_PIN_LED_RGB   48   // LED WS2812 integrato
#define VOS_PIN_BOOT       0   // pulsante BOOT

// Stato della rete (usato anche per il colore del LED)
enum NetState : uint8_t {
  NET_BOOT = 0,
  NET_AP,
  NET_CLIENT_TRY,
  NET_CLIENT_OK
};

// Modalita del LED
enum LedMode : uint8_t {
  LED_STATE = 0,   // colore = stato del sistema
  LED_HEARTBEAT,   // battito legato al carico CPU
  LED_FIXED,       // colore fisso
  LED_OFF
};
