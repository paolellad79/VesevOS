// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_wd.h
// Watchdog: controlla che i servizi diano segni di vita, che la rete ci sia, che la RAM basti;
// riavvio programmato (orario e giorni) o dopo N giorni. Antiloop: dopo 3 riavvii automatici in un'ora non riavvia piu ed entra in modalita ridotta (spegne il servizio bloccato o i servizi opzionali).
#pragma once
#include <stddef.h>
#include <stdint.h>
#include <string>
#include "vos_heapbuf.h"
#include "vos_wd_limits.h"

void   wdInit();                              // avvia il task "wd"
void   wdBeat(const char* task);              // "sono vivo" (da chiamare nel ciclo di ogni task controllato)
void   wdWatch(const char* task, uint16_t timeoutSec);   // controlla questo task
uint16_t wdLimit(const char* task);              // limite scelto per quel servizio (secondi): valore di base o quello salvato in config
void   wdRetime(const char* task, uint16_t oldSec, uint16_t newSec);   // cambia il limite di un servizio gia controllato, subito (salvo allungamento temporaneo)
void   wdUnwatch(const char* task);           // non controllare piu (task fermato apposta)
#define WD_JSON_MAX 2560
bool   wdJson(HeapBuf& out);
std::string wdText();
void   wdStalest(char* out, size_t n);        // servizio con il battito piu vecchio ("nome 12s"), per il diario
const char* wdLastReason();                    // motivo dell'ultimo riavvio automatico ("" se nessuno)
void   wdReboot(const char* why);              // riavvio AUTOMATICO: conta per l'antiloop, poi sysReboot
void   sysRebootLater(const char* why, uint32_t waitMs);   // come sysReboot ma senza bloccare chi chiama (task a parte): per rispondere prima alla pagina; why = testo fisso (letterale)
void   sysReboot(const char* why, uint32_t waitMs);   // UNICO punto di riavvio: motivo nel diario dei riavvii, attesa, poi il driver
