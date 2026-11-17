// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_diag.h
// Diagnostica con i lampi del LED: classe e dettaglio dell'errore (regole in vos_diag_rule.h, piano in build/analisi/diagnostica-led-2026-11-16.md).
// Qui: causa dell'ultimo riavvio, errori che durano, e il "motore" che dice ai task LED se il LED e acceso o spento in questo momento.
// Il LED vero lo pilotano devices/vos_led.cpp (LED a colori, S3) e devices/vos_led2.cpp (LED semplice, C3 e C6).
#pragma once
#include <stdint.h>
#include <string>
#include "vos_diag_rule.h"

void diagBoot();                                  // dopo crashEarly() e logInit(): classifica la causa dell'ultimo riavvio
void diagSegnala(uint8_t classe, uint8_t dett);   // errore che dura (NVS, file system): un giro ogni 60 s finche la scheda non si riavvia
void diagProva(uint8_t classe, uint8_t dett);     // prova dal comando "diag led": un giro del codice, anche con il LED spento
std::string diagTesto();                          // testo del comando "diag codici"

// Chiamata da UN solo task LED per scheda: vero = la diagnostica comanda il LED adesso e *acceso dice come (ms = orologio in ms, ledSpento = LED spento dalle impostazioni).
// *r,*g,*b = colore consigliato per il LED a colori (il LED semplice lo ignora; puo essere nullptr).
bool diagLedComanda(uint32_t ms, bool ledSpento, bool* acceso, uint8_t* r, uint8_t* g, uint8_t* b);
