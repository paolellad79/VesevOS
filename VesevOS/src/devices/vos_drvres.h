// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_drvres.h
// STRATO 4 (devices): RISORSE dei driver ELF (.vde): i pin che l'utente ha dato a ogni driver, con il suo bus (gpio, i2c, spi).
// Il driver PROPONE i pin (segnali del suo file), l'UTENTE li sceglie e li applica; il controllo (pin vietati, gia usati, doppi) e quello
// unico del registro dei pin (vos_pins: pinTestBlock e pinClaim). Un driver non ha pin che l'utente non gli ha dato.
// Salvate in /drvres.cfg (una riga per driver). Il nome del driver e anche il nome dell'istanza che i moduli mettono in use=.
// Solo con i moduli ELF (VOS_WITH_ELF). Max DRVRES_MAX driver.
#pragma once
#include "../core/vos_common.h"
#include "../core/vos_errbuf.h"
#include "../core/vos_heapbuf.h"

#if VOS_WITH_ELF
#define DRVRES_MAX      4
#define DRVRES_PINS_MAX 4
enum DrvResBus : uint8_t { DRB_NONE = 0, DRB_GPIO = 1, DRB_I2C = 2, DRB_SPI = 3 };   // I2C: pin[0] = SDA, pin[1] = SCL. SPI: CS, MOSI, MISO, SCK

struct DrvRes {
  char     name[25];
  uint8_t  bus;
  uint8_t  npin;
  int8_t   pin[DRVRES_PINS_MAX];
  uint8_t  addr;               // solo I2C: indirizzo a 7 bit
  uint16_t khz;                // I2C: 100 o 400; SPI: da 100 a 10000
};

void drvresInit();                                        // legge /drvres.cfg e prende i pin nel registro (una riga con un pin occupato resta nel file)
int  drvresCount();
bool drvresGet(int i, DrvRes& out);                       // copia (il blocco non resta preso)
bool drvresFind(const char* name, DrvRes& out);
const char* drvresBusName(uint8_t bus);                   // "gpio" | "i2c" | "spi"
// Assegna i pin a un driver. false = err dice perche (pin vietato/usato/doppio, driver gia presente, nome di una periferica, file non scritto).
bool drvresAdd(const char* name, uint8_t bus, const int* pins, int npin, int addr, int khz, ErrBuf& err);
bool drvresRemove(const char* name, ErrBuf& err);        // toglie e libera i pin; se non c'e: err
#endif
