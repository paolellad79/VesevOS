// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_bmp280.h
// Sensore BMP280 (temperatura e pressione) su I2C, indirizzo 0x76 o 0x77. Il BME280 (chip id 0x60) si legge allo stesso modo, ma senza umidita.
// Il calcolo e quello intero del foglio dati Bosch (compensazione a 32/64 bit). Il bus passa solo da vos_drv_i2c.
#pragma once
#include <stdint.h>
#include <stddef.h>

struct Bmp280Cal { uint16_t T1; int16_t T2, T3; uint16_t P1; int16_t P2, P3, P4, P5, P6, P7, P8, P9; };

enum { BMP_OK = 0, BMP_NOT_READY = 1, BMP_NO_ANSWER = 2, BMP_WRONG_CHIP = 3, BMP_BUS = 4 };

void bmp280ParseCal(const uint8_t* raw24, Bmp280Cal& c);                                             // registri 0x88..0x9F
void bmp280Compensate(const Bmp280Cal& c, int32_t adcT, int32_t adcP, int32_t& t100, uint32_t& pPa);  // t100: centesimi di grado; pPa: pascal
int  bmp280Begin(uint8_t addr, Bmp280Cal& c, uint8_t& chipId);                                       // controlla il chip, legge la taratura, avvia le misure
int  bmp280Sample(uint8_t addr, const Bmp280Cal& c, int32_t& t100, uint32_t& pPa);                   // BMP_NOT_READY = la prima misura non e ancora pronta
