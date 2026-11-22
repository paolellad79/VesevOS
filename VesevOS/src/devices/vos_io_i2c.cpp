// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_io_i2c.cpp
// Misura del sensore I2C (BMP280) delle periferiche a classi: stato, ripetizione dopo gli errori e ultima misura. Spostato da vos_io.cpp
// (file sotto le 500 righe), stesso comportamento. ioI2cTick gira nel loop dentro il blocco di vos_io.
#include "vos_io.h"
#include "vos_io_int.h"
#include "vos_bmp280.h"
#include "../core/vos_taratura.h"
#include "../core/vos_config.h"
#include "../drivers/vos_drv_i2c.h"

// un solo sensore I2C: misura nel loop (ioI2cTick), la pagina e le app leggono solo l'ultimo valore
static struct I2cState { bool up, have, armed; uint8_t fails, err, chip; int32_t t100; uint32_t pPa; uint32_t nextAt; Bmp280Cal cal; } g_i2c;
static bool g_i2cBus = false;
static uint32_t g_i2cKey = 0;                      // SDA, SCL e velocita con cui il bus e acceso

void ioI2cReset() { g_i2c = I2cState(); }                                  // il bus lo avvia ioI2cTick
bool ioI2cHave() { return g_i2c.have; }
// Il valore grezzo resta in g_i2c.t100; la taratura si applica qui, una volta sola, per tutti (pagina, app, comandi, registratore)
void ioI2cRead(bool& have, int32_t& t100, uint32_t& pPa) {
  have = g_i2c.have;
  t100 = tempCorrettaCent(g_i2c.t100, (int)cfg.tempTarExt);
  pPa = g_i2c.pPa;
}

static void i2cFail(uint32_t now, int r) {
  g_i2c.up = false; g_i2c.err = (uint8_t)r; if (g_i2c.fails < 250) g_i2c.fails++;
  if (g_i2c.fails >= 3) g_i2c.have = false;
  uint32_t wait = 2000UL << (g_i2c.fails < 4 ? g_i2c.fails : 4);                  // 4, 8, 16, 32 s: un sensore staccato non occupa il loop
  g_i2c.nextAt = now + (wait > 30000UL ? 30000UL : wait); g_i2c.armed = true;
}
void ioI2cTick(uint32_t now) {
  const IoDev* d = nullptr;
  for (int i = 0; i < ioCount(); i++) if (ioDevAt(i)->cls == IC_I2C) { d = ioDevAt(i); break; }
  if (!d) { if (g_i2cBus) { drvI2cEnd(); g_i2cBus = false; } return; }
  if (g_i2c.armed && (int32_t)(now - g_i2c.nextAt) < 0) return;
  if (!g_i2c.up) {
    uint32_t key = (uint32_t)(uint8_t)d->gpio | ((uint32_t)(uint8_t)d->gpio2 << 8) | ((d->flags & IOF_FAST) ? 0x10000UL : 0);
    if (g_i2cBus && key != g_i2cKey) { drvI2cEnd(); g_i2cBus = false; }            // sensore tolto e rimesso su altri pin
    if (!g_i2cBus) {
      if (!drvI2cBegin(d->gpio, d->gpio2, (d->flags & IOF_FAST) ? 400000UL : 100000UL)) { i2cFail(now, BMP_BUS); return; }
      g_i2cBus = true; g_i2cKey = key;
    }
    int r = bmp280Begin(d->addr, g_i2c.cal, g_i2c.chip);
    if (r) { i2cFail(now, r); return; }
    g_i2c.up = true; g_i2c.fails = 0; g_i2c.err = 0;
    g_i2c.nextAt = now + 300; g_i2c.armed = true;                                // la prima misura e pronta dopo qualche ms
    return;
  }
  int32_t t; uint32_t p;
  int r = bmp280Sample(d->addr, g_i2c.cal, t, p);
  if (r == BMP_NOT_READY) { g_i2c.nextAt = now + 300; g_i2c.armed = true; return; }
  if (r) { i2cFail(now, r); return; }
  g_i2c.t100 = t; g_i2c.pPa = p; g_i2c.have = true; g_i2c.fails = 0; g_i2c.err = 0;
  g_i2c.nextAt = now + 2000; g_i2c.armed = true;
}
