// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_wd.h
// Watchdog: controlla che i servizi diano segni di vita, che la rete ci sia, che la RAM basti;
// riavvio programmato (orario e giorni) o dopo N giorni. Antiloop: dopo 3 riavvii automatici in un'ora non riavvia piu ed entra in modalita ridotta (spegne il servizio bloccato o i servizi opzionali).
#pragma once
#include <Arduino.h>

void   wdInit();                              // avvia il task "wd"
void   wdBeat(const char* task);              // "sono vivo" (da chiamare nel ciclo di ogni task controllato)
void   wdWatch(const char* task, uint16_t timeoutSec);   // controlla questo task
void   wdUnwatch(const char* task);           // non controllare piu (task fermato apposta)
String wdJson();
String wdText();
void   wdStalest(char* out, size_t n);        // servizio con il battito piu vecchio ("nome 12s"), per il diario
String wdLastReason();                        // motivo dell'ultimo riavvio automatico ("" se nessuno)
void   wdReboot(const String& why);           // riavvio con motivo salvato (anche da altri moduli)
