// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_drv_spi.h
// STRATO 1 (driver): unico file che usa il bus SPI per i driver ELF (driver/spi_master.h, SPI3: l'SPI2 e della microSD). Modo 0, MSB per primo.
// Il pin CS lo muove questo driver (basso durante lo scambio, alto a riposo): un solo dispositivo per bus. Blocchi fino a DRV_SPI_MAX byte.
// Va chiamato da UN solo task alla volta (il blocco lo tiene chi chiama: packages/vos_hw.cpp).
#pragma once
#include <stdint.h>
#include <stddef.h>

#define DRV_SPI_MAX 64

bool drvSpiBegin(int cs, int mosi, int miso, int sck, uint32_t khz);   // avvia il bus (rilascia quello di prima); false = non parte
void drvSpiEnd();                                                      // ferma il bus e libera i pin
// Scambia n byte. tx puo essere nullptr (si mandano zeri), rx puo essere nullptr (la risposta si butta). true = riuscito.
bool drvSpiTransfer(const uint8_t* tx, uint8_t* rx, size_t n);
