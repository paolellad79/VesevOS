// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_cpuboost.h
// Prenotazioni di velocita: "serve la CPU al massimo per qualche secondo" (scansione Wi-Fi, script Berry...).
// Chiedono ai governor ondemand, conservative e schedutil di alzare il tetto a 240 MHz fino alla scadenza.
// Non toccano l'hardware (nessun esp_pm qui): si possono chiamare da qualsiasi task, anche da quello di rete.
// Con una velocita fissa (80, 160, 240) e con il caldo la prenotazione non conta.
#pragma once
#include <stdint.h>

static const uint32_t CPUBOOST_MAX_SEC = 30;      // una prenotazione dura al massimo tanto (si rinnova chiamando di nuovo)

// who = nome breve di chi prenota (si vede in "cpu why"). sec oltre il massimo viene limitato.
void        cpuBoost(const char* who, uint32_t sec);
bool        cpuBoostActive();                     // c'e una prenotazione non ancora scaduta?
const char* cpuBoostWho();                        // chi ha prenotato per ultimo ("" se nessuno)
