// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_log.h
// Log circolare in RAM, testo sempre pulito (ASCII). Ogni riga porta il livello: [E] [W] [I] [D].
#pragma once
#include <Arduino.h>

void   logInit();
enum LogLv { LG_ERR = 0, LG_WARN = 1, LG_INFO = 2, LG_DBG = 3 };
void   vlog(const char* fmt, ...);     // livello deciso dal testo: ERRORE / AUDIT: ROSSO = errore, ATTENZIONE / AUDIT: GIALLO = attenzione, altrimenti info
void   vlogl(int lv, const char* fmt, ...);   // livello esplicito (LG_DBG per i dettagli: spenti di fabbrica)
void   logSetLevel(int lv);            // cosa si registra: 0 errori, 1 + attenzioni, 2 + info (di fabbrica), 3 + dettagli
int    logLevel();
String logGet(int maxLines);           // ultime righe, separate da \n
void   logClear();                     // svuota il registro in RAM
