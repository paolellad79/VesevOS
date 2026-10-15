// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_drv_i2c.h
// STRATO 1 (driver): unico file che usa il bus I2C (Wire). Pin liberi a scelta (l'ESP32-S3 li instrada via GPIO matrix).
// Ogni transazione ha un tempo massimo di 20 ms: un bus bloccato non ferma il firmware. Va chiamato da UN solo task (il loop).
#pragma once
#include <Arduino.h>

enum DrvI2cErr : uint8_t { DI_OK = 0, DI_NACK = 2, DI_NACK_DATA = 3, DI_OTHER = 4, DI_TIMEOUT = 5, DI_SHORT = 7, DI_NOBUS = 8 };

bool drvI2cBegin(int sda, int scl, uint32_t hz);                                         // avvia il bus (hz: 100000 o 400000)
void drvI2cEnd();                                                                        // ferma il bus e libera i pin
uint8_t drvI2cReadReg(uint8_t addr, uint8_t reg, uint8_t* buf, size_t n);                // DI_OK oppure il codice dell'errore
uint8_t drvI2cWriteReg(uint8_t addr, uint8_t reg, const uint8_t* buf, size_t n);
