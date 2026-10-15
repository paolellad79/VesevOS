// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_drv_i2c.cpp
#include "vos_drv_i2c.h"
#include <Wire.h>

static bool g_up = false;

bool drvI2cBegin(int sda, int scl, uint32_t hz) {
  if (g_up) { Wire.end(); g_up = false; }
  if (!Wire.begin(sda, scl, hz)) return false;
  Wire.setTimeOut(20);
  g_up = true;
  return true;
}
void drvI2cEnd() { if (g_up) { Wire.end(); g_up = false; } }

uint8_t drvI2cReadReg(uint8_t addr, uint8_t reg, uint8_t* buf, size_t n) {
  if (!g_up) return DI_NOBUS;
  if (!n || n > 32) return DI_OTHER;
  Wire.beginTransmission(addr);
  Wire.write(reg);
  uint8_t e = Wire.endTransmission(false);
  if (e) return e;
  size_t got = Wire.requestFrom(addr, n, true);
  if (got != n) { while (Wire.available()) Wire.read(); return DI_SHORT; }
  for (size_t i = 0; i < n; i++) buf[i] = (uint8_t)Wire.read();
  return DI_OK;
}
uint8_t drvI2cWriteReg(uint8_t addr, uint8_t reg, const uint8_t* buf, size_t n) {
  if (!g_up) return DI_NOBUS;
  if (n > 30) return DI_OTHER;
  Wire.beginTransmission(addr);
  Wire.write(reg);
  for (size_t i = 0; i < n; i++) Wire.write(buf[i]);
  return Wire.endTransmission(true);
}
