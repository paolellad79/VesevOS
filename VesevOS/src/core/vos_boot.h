// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_boot.h
// Ordine di avvio dei servizi, con dipendenze.
#pragma once
#include <Arduino.h>

void   bootRun();                                              // avvia i servizi nell'ordine scelto (da setup)
void   bootStable();                                           // da chiamare nel loop: dopo 60 s l'avvio e riuscito
String bootJson();                                             // ordine, dipendenze, tempi (per la pagina)
bool   bootSetOrder(const String& csv, String& result, String& err);   // salva (correggendo con le dipendenze)
void   bootResetOrder();                                       // torna all'ordine predefinito
