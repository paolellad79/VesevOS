// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_hw.cpp (vedi vos_hw.h).
#include "vos_hw.h"

#if VOS_WITH_ELF
#include <stdio.h>
#include <stdio.h>
#include <string.h>
#include "../devices/vos_drvres.h"
#include "../devices/vos_io.h"
#include "../drivers/vos_drv_gpio.h"
#include "../drivers/vos_drv_i2c.h"
#include "../drivers/vos_drv_spi.h"
#include "../drivers/vos_drv_time.h"
#include "../core/vos_lock.h"

namespace {

VosMutex& mx() { static VosMutex m = [] { VosMutex v; v.init(); return v; }(); return m; }

char    g_i2cDi[25];            // driver che ha acceso il bus I2C ("" = nessuno)
char    g_spiDi[25];
struct { char nome[25]; uint8_t out; } g_gp[DRVRES_MAX];   // per driver: quali pin sono stati messi in uscita (bit = idx)

uint8_t* maskDi(const char* drv) {
  for (auto& g : g_gp) if (!strcmp(g.nome, drv)) return &g.out;
  for (auto& g : g_gp) if (!g.nome[0]) { snprintf(g.nome, sizeof g.nome, "%s", drv); g.out = 0; return &g.out; }
  return nullptr;
}

bool trova(const char* drv, uint8_t bus, DrvRes& r) {
  return drv && drvresFind(drv, r) && r.bus == bus;
}

bool i2cPronto(const char* drv, const DrvRes& r) {
  if (!strcmp(g_i2cDi, drv)) return true;
  if (ioHasI2c()) return false;                              // il bus e del sensore delle periferiche
  if (!drvI2cBegin(r.pin[0], r.pin[1], (uint32_t)r.khz * 1000)) { g_i2cDi[0] = 0; return false; }
  snprintf(g_i2cDi, sizeof g_i2cDi, "%s", drv);
  return true;
}

}  // namespace

int hwGpioMode(const char* drv, int idx, int modo) {
  DrvRes r;
  if (modo < HW_MODO_IN || modo > HW_MODO_OUT) return HW_E_ARG;
  if (!trova(drv, DRB_GPIO, r)) return HW_E_NONTROVATO;
  if (idx < 0 || idx >= r.npin) return HW_E_ARG;
  VosLock L(mx());
  uint8_t* out = maskDi(drv);
  if (!out) return HW_E_OCCUPATO;
  if (modo == HW_MODO_OUT) *out |= (uint8_t)(1u << idx); else *out &= (uint8_t)~(1u << idx);
  drvGpioMode(r.pin[idx], (DrvGpioMode)modo);                // DG_* ha gli stessi numeri
  return 0;
}

int hwGpioRead(const char* drv, int idx) {
  DrvRes r;
  if (!trova(drv, DRB_GPIO, r)) return HW_E_NONTROVATO;
  if (idx < 0 || idx >= r.npin) return HW_E_ARG;
  return drvGpioRead(r.pin[idx]) ? 1 : 0;
}

int hwGpioWrite(const char* drv, int idx, int livello) {
  DrvRes r;
  if (!trova(drv, DRB_GPIO, r)) return HW_E_NONTROVATO;
  if (idx < 0 || idx >= r.npin) return HW_E_ARG;
  VosLock L(mx());
  uint8_t* out = maskDi(drv);
  if (!out || !(*out & (1u << idx))) return HW_E_ARG;        // prima il modo: un pin non si scrive per sbaglio
  drvGpioWrite(r.pin[idx], livello != 0);
  return 0;
}

int hwI2cProbe(const char* drv) {
  DrvRes r;
  if (!trova(drv, DRB_I2C, r)) return HW_E_NONTROVATO;
  VosLock L(mx());
  if (!i2cPronto(drv, r)) return HW_E_OCCUPATO;
  return drvI2cProbe(r.addr) ? 1 : 0;
}

int hwI2cRead(const char* drv, int reg, uint8_t* buf, int n) {
  DrvRes r;
  if (!buf || reg < 0 || reg > 255 || n < 1) return HW_E_ARG;
  if (n > HW_I2C_MAX) return HW_E_GRANDE;
  if (!trova(drv, DRB_I2C, r)) return HW_E_NONTROVATO;
  VosLock L(mx());
  if (!i2cPronto(drv, r)) return HW_E_OCCUPATO;
  return drvI2cReadReg(r.addr, (uint8_t)reg, buf, (size_t)n) == DI_OK ? n : HW_E_IO;
}

int hwI2cWrite(const char* drv, int reg, const uint8_t* buf, int n) {
  DrvRes r;
  if (reg < 0 || reg > 255 || n < 0 || (n > 0 && !buf)) return HW_E_ARG;
  if (n > HW_I2C_MAX - 2) return HW_E_GRANDE;
  if (!trova(drv, DRB_I2C, r)) return HW_E_NONTROVATO;
  VosLock L(mx());
  if (!i2cPronto(drv, r)) return HW_E_OCCUPATO;
  return drvI2cWriteReg(r.addr, (uint8_t)reg, buf, (size_t)n) == DI_OK ? n : HW_E_IO;
}

int hwSpiTransfer(const char* drv, const uint8_t* tx, uint8_t* rx, int n) {
  DrvRes r;
  if (n < 1) return HW_E_ARG;
  if (n > HW_SPI_MAX) return HW_E_GRANDE;
  if (!trova(drv, DRB_SPI, r)) return HW_E_NONTROVATO;
  VosLock L(mx());
  if (strcmp(g_spiDi, drv)) {                                // pin del driver: CS, MOSI, MISO, SCK
    if (!drvSpiBegin(r.pin[0], r.pin[1], r.pin[2], r.pin[3], r.khz)) { g_spiDi[0] = 0; return HW_E_OCCUPATO; }
    snprintf(g_spiDi, sizeof g_spiDi, "%s", drv);
  }
  return drvSpiTransfer(tx, rx, (size_t)n) ? n : HW_E_IO;
}

int hwDelayUs(int us) {
  if (us <= 0) return 0;
  if (us > HW_DELAY_US_MAX) us = HW_DELAY_US_MAX;
  drvDelayUs((uint32_t)us);
  return us;
}

uint32_t hwUptime() { return (uint32_t)(drvMillis() / 1000); }

void hwRilascia(const char* drv) {
  if (!drv) return;
  VosLock L(mx());
  if (!strcmp(g_i2cDi, drv)) { drvI2cEnd(); g_i2cDi[0] = 0; }
  if (!strcmp(g_spiDi, drv)) { drvSpiEnd(); g_spiDi[0] = 0; }
  DrvRes r;
  uint8_t* out = maskDi(drv);
  if (out && drvresFind(drv, r) && r.bus == DRB_GPIO) {
    for (int i = 0; i < r.npin; i++) if (*out & (1u << i)) drvGpioMode(r.pin[i], DG_INPUT);   // le uscite tornano in ingresso
  }
  for (auto& g : g_gp) if (!strcmp(g.nome, drv)) memset(&g, 0, sizeof g);
}
#endif
