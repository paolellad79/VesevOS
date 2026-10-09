// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_ram.h
// Misura della RAM (passo R0 del piano RAM): riepilogo, stack di ogni task con il margine che si puo togliere,
// e confronto prima/dopo (`ram mark` ... `ram diff`) per sapere quanto costa accendere un servizio.
#pragma once
#include <Arduino.h>

String ramReport();     // RAM interna e PSRAM, poi i task con stack libero minimo, totale e margine recuperabile
void   ramMark();       // ricorda la RAM di adesso
String ramDiff();       // cosa e cambiato dall'ultimo ramMark ("" se non c'e un segno)

bool   ramTlsToPsram();  // i blocchi grandi della libreria TLS (HTTPS) vanno in PSRAM; true = riuscito
int    ramTlsState();    // 0 = non provato, 1 = attivo, -1 = non disponibile in questo core
