// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_log.h
// Log circolare in RAM, testo sempre pulito (ASCII).
#pragma once
#include <Arduino.h>

void   logInit();
void   vlog(const char* fmt, ...);     // scrive su seriale + buffer
String logGet(int maxLines);           // ultime righe, separate da \n
