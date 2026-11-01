// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_prog_kind.h
// STRATO 6 (packages): i tipi di programma. Sta in un file a parte perche chi guarda solo il tipo (per esempio vos_prog_data.h) non deve tirarsi dietro
// tutto vos_prog.h (che ha dentro anche le app e il motore Berry: sarebbe un ciclo tra moduli).
#pragma once
#include <stdint.h>

enum ProgKind : uint8_t { PK_SCRIPT = 0, PK_APP_B = 1, PK_APP_E = 2 };   // uguale a cfg.boot[].kind
