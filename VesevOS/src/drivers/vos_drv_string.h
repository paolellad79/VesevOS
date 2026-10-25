// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_drv_string.h
// STRATO 1 (driver): i tipi e le funzioni standard di C/C++ che prima arrivavano da Arduino.h (stdint, string.h, stdio...).
// Niente String di Arduino: il testo e std::string, std::string_view o buffer fissi. Gli altri file includono questo header, mai Arduino.h.
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <ctype.h>

#include <string_view>

// Sull'ESP32 i dati costanti stanno gia in flash: PROGMEM non fa niente (i file generati lo scrivono ancora)
#ifndef PROGMEM
#define PROGMEM
#endif
