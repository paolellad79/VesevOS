// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_drv_gpio.h
// STRATO 1 (driver): unico file che usa i GPIO e l'ADC (driver/gpio.h, esp_adc/adc_oneshot.h).
// Non sa chi usa il pin: il registro dei pin (vos_pins) tiene i conti, i servizi decidono cosa fare.
#pragma once
#include <stdint.h>

enum DrvGpioMode : uint8_t { DG_INPUT = 0, DG_INPUT_PULLUP = 1, DG_INPUT_PULLDOWN = 2, DG_OUTPUT = 3 };

enum DrvGpioRes : uint8_t { DG_RES_NESSUNA = 0, DG_RES_SU = 1, DG_RES_GIU = 2 };

void drvGpioMode(int gpio, DrvGpioMode m);   // DG_OUTPUT lascia anche la lettura attiva (si puo rileggere il livello scritto)
void drvGpioResistenze(int gpio, bool su, bool giu);   // solo le resistenze digitali (pull-up e pull-down), senza toccare direzione e collegamenti del piedino: serve a chi ha gia configurato il piedino (PCNT)
void drvGpioRilascia(int gpio);                         // riporta il piedino allo stato di partenza (lo si usa quando un bus lo lascia libero)
void drvGpioWrite(int gpio, bool high);
bool drvGpioRead(int gpio);
int  drvGpioAnalog(int gpio);          // lettura analogica grezza 0..4095, 12 bit, 12 dB (ADC1: GPIO1-10 sul S3); 0 se il pin non e analogico
int  drvGpioAnalogMv(int gpio);        // come sopra, ma in millivolt con la calibrazione di fabbrica (ESP-IDF); -1 se non disponibile
bool drvGpioCanaleAdc(int gpio, int& unita, int& canale);   // dal piedino all'ADC: unita 1 o 2, canale; false se il piedino non e analogico (non configura niente)

// Resistenze e risveglio nel dominio RTC (servono al sonno: nel sonno vale la resistenza RTC, non quella digitale)
bool drvGpioHaRtc(int gpio);                         // il piedino ha la funzione RTC: puo svegliare il sonno
void drvGpioRtcResistenza(int gpio, bool alto);      // alto = si attende il livello alto: pull-down RTC; altrimenti pull-up RTC
void drvGpioRtcRilascia(int gpio);                   // dopo il risveglio: toglie l'hold RTC e riporta il piedino digitale
void drvGpioRisveglioLivello(int gpio, bool alto);   // arma il risveglio del sonno leggero dal livello del piedino (GPIO wakeup)
void drvGpioRisveglioSpegni(int gpio);               // disarma quel risveglio
void drvGpioRtcIngresso(int gpio, DrvGpioRes r);   // piedino RTC in ingresso con la resistenza voluta, congelato (hold) per il sonno
