// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_drv_gpio.cpp
#include "vos_drv_gpio.h"

void drvGpioMode(int gpio, DrvGpioMode m) {
  pinMode(gpio, m == DG_OUTPUT ? OUTPUT : m == DG_INPUT_PULLUP ? INPUT_PULLUP : m == DG_INPUT_PULLDOWN ? INPUT_PULLDOWN : INPUT);
}
void drvGpioWrite(int gpio, bool high) { digitalWrite(gpio, high ? HIGH : LOW); }
bool drvGpioRead(int gpio) { return digitalRead(gpio) != LOW; }
