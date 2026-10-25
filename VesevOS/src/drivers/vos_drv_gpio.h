// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_drv_gpio.h
// STRATO 1 (driver): unico file che usa i GPIO e l'ADC (driver/gpio.h, esp_adc/adc_oneshot.h).
// Non sa chi usa il pin: il registro dei pin (vos_pins) tiene i conti, i servizi decidono cosa fare.
#pragma once
#include <stdint.h>

enum DrvGpioMode : uint8_t { DG_INPUT = 0, DG_INPUT_PULLUP = 1, DG_INPUT_PULLDOWN = 2, DG_OUTPUT = 3 };

void drvGpioMode(int gpio, DrvGpioMode m);   // DG_OUTPUT lascia anche la lettura attiva (si puo rileggere il livello scritto)
void drvGpioWrite(int gpio, bool high);
bool drvGpioRead(int gpio);
int  drvGpioAnalog(int gpio);          // lettura analogica grezza 0..4095, 12 bit, 12 dB (ADC1: GPIO1-10 sul S3); 0 se il pin non e analogico
int  drvGpioAnalogMv(int gpio);        // come sopra, ma in millivolt con la calibrazione di fabbrica (ESP-IDF); -1 se non disponibile
