// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_aggiorna_int.h
// Tra i file della gestione degli aggiornamenti.
#pragma once
#include <stddef.h>
#include "../core/vos_heapbuf.h"

enum AggNota : int { AGG_NOTA_NIENTE = 0, AGG_NOTA_DISPONIBILE, AGG_NOTA_INSTALLATO, AGG_NOTA_FALLITO };

bool aggPacchettiJson(HeapBuf& j);                  // app e driver installati: il confronto con lo store lo fa la pagina
const char* aggFileRiserva();                       // percorso della copia di riserva sulla microSD
