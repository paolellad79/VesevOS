// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_boot.h
// Ordine di avvio dei servizi, con dipendenze.
#pragma once
#include "vos_errbuf.h"
#include <stddef.h>
#include <stdint.h>
#include "vos_heapbuf.h"

void   bootRun();                                              // avvia i servizi nell'ordine scelto (da setup)
void   bootStable();                                           // da chiamare nel loop: dopo 60 s l'avvio e riuscito
#define BOOT_JSON_MAX 2048
bool   bootJson(HeapBuf& out);                                 // ordine, dipendenze, tempi (per la pagina)
bool   bootSetOrder(const char* csv, char* result, size_t resultLen, ErrBuf& err);   // salva (correggendo con le dipendenze)
void   bootResetOrder();                                       // torna all'ordine predefinito
