// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_drv_led.h
// STRATO 1 (driver): LED RGB WS2812 su un pin, pilotato con il periferico RMT di ESP-IDF (codice nostro, nessun componente esterno).
// Va chiamato da UN solo task (quello del LED). Il pin puo cambiare: il canale si ricrea da solo.
#pragma once
#include <stdint.h>

void drvLedWrite(int gpio, uint8_t r, uint8_t g, uint8_t b);   // colore 0..255 per canale (l'ordine dei fili del WS2812 e gestito qui)
