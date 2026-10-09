// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_drv_gpio.h
// STRATO 1 (driver): unico file che usa pinMode / digitalWrite / digitalRead (i pin digitali della scheda).
// Non sa chi usa il pin: il registro dei pin (vos_pins) tiene i conti, i servizi decidono cosa fare.
#pragma once
#include <Arduino.h>

enum DrvGpioMode : uint8_t { DG_INPUT = 0, DG_INPUT_PULLUP = 1, DG_INPUT_PULLDOWN = 2, DG_OUTPUT = 3 };

void drvGpioMode(int gpio, DrvGpioMode m);
void drvGpioWrite(int gpio, bool high);
bool drvGpioRead(int gpio);
