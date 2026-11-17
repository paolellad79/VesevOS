// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_drv_pwm.h
// STRATO 1 (driver): unico file che usa il controllore PWM per LED (LEDC, driver/ledc.h). Gestisce un elenco di canali
// (6 o 8, secondo il chip) e di timer (4): due canali con la stessa frequenza dividono lo stesso timer.
// Clienti: il LED2 (4 kHz, 8 bit) e le periferiche PWM dell'utente (vos_io). Orologio del quarzo: la frequenza non cambia
// con la velocita della CPU. Il controllo dei pin (vietati, occupati) NON e qui: lo fa il registro dei pin.
// Le funzioni non usate non entrano nel binario (il linker le scarta).
#pragma once
#include <stdint.h>

#define DRV_PWM_HZ_MIN 10
#define DRV_PWM_HZ_MAX 40000

// Codici di errore di drvPwmOpen (numeri negativi). I testi li scrive chi chiama, con tr().
#define DRV_PWM_E_CANALI  (-1)    // nessun canale libero
#define DRV_PWM_E_TIMER   (-2)    // nessun timer libero per questa frequenza
#define DRV_PWM_E_HZ      (-3)    // frequenza che il chip non sa fare
#define DRV_PWM_E_HW      (-4)    // il controllore ha rifiutato la configurazione

// Prenota un canale sul pin e lo porta al livello di spento. bits 0 = risoluzione scelta dal driver (la piu alta possibile, al massimo 14).
// activeLow: lo "spento" e il livello alto. Ritorna il numero del canale (>= 0) oppure un codice DRV_PWM_E_*.
int drvPwmOpen(int gpio, uint32_t hz, uint8_t bits, bool activeLow);

// Porta l'uscita a parte/su della luminosita piena (per esempio 50/100 oppure 128/255). 0 = spento, parte >= su = sempre acceso.
void drvPwmSet(int handle, uint32_t parte, uint32_t su);

// Ferma il canale (il pin resta sul livello di spento) e lo restituisce. Il timer si libera quando nessuno lo usa piu.
void drvPwmClose(int handle);

// Canali ancora liberi e canali totali del chip.
int drvPwmFree();
int drvPwmTotal();
