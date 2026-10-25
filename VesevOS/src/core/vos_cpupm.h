// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_cpupm.h
// Velocita della CPU: scelta dell'utente (80, 160, 240 fissi, oppure un governor: ondemand, conservative, schedutil),
// riduzione per il caldo e pavimento del TETTO a 160 con HTTPS (a riposo la CPU resta a 80). Il monitor chiama cpuPmTick() una volta al secondo: e l'UNICO punto che
// cambia la frequenza (drivers/vos_drv_cpu.*). I governor stanno in vos_cpugov.*, le prenotazioni in vos_cpuboost.*.
#pragma once
#include <stdint.h>
#include <stddef.h>
#include "vos_errbuf.h"

// Stato termico: 0 normale, 1 avviso, 2 velocita ridotta, 3 caldo estremo.
enum CpuThermal : uint8_t { THERM_OK = 0, THERM_WARN = 1, THERM_REDUCED = 2, THERM_EXTREME = 3 };

// Perche il tetto e quello che e.
enum CpuWhy : uint8_t {
  CW_START = 0,    // primi secondi dopo l'avvio: nessun cambio
  CW_USER,         // velocita fissa scelta dall'utente
  CW_LOAD,         // lo decide il governor in base al carico
  CW_BOOST,        // prenotazione (scansione Wi-Fi, script...)
  CW_HOT,          // abbassato per il caldo
  CW_HTTPS,        // caldo, ma HTTPS acceso: il tetto resta 160
  CW_SENSOR        // sensore di temperatura in errore: tetto prudente
};

// ---- scelta e parametri (vos_cpuset.cpp) ----
// Scelta da testo: "80", "160", "240", "ondemand", "conservative", "schedutil" ("auto" vale ondemand, per i vecchi client).
// Salva la config e la fa valere. false = testo non ammesso (nulla cambia).
bool        cpuChoiceSet(const char* text);
bool        cpuChoiceValid(const char* text);
const char* cpuChoiceName();                 // scelta in vigore, come testo
bool        cpuModeValid(int mode);          // vecchio formato: 0 (governor ondemand), 80, 160, 240
bool        cpuModeSet(int mode);            // idem, da numero (vecchio parametro "mode" dell'API)
// Parametro dei governor: nome "up", "down" o "hold". false e err = valore fuori campo.
bool        cpuParamSet(const char* name, long value, ErrBuf& err);
// Tutti e tre insieme (controllati insieme: la discesa deve restare 10 sotto la salita).
bool        cpuParamsSet(int up, int down, int hold, ErrBuf& err);

// ---- motore (vos_cpupm.cpp) ----
void        cpuPmApplyMode();                // la scelta e cambiata: il monitor la applica al prossimo secondo
// Da chiamare una volta al secondo dal monitor: tempC = temperatura (NAN = sensore in errore), busyPct = carico del core piu occupato.
void        cpuPmTick(float tempC, int busyPct);
CpuThermal  cpuPmThermal();                  // stato termico attuale
const char* cpuPmThermalText(CpuThermal t);  // lo stato termico in parole (tradotto): normale, avviso, ridotta, estremo
// Le soglie di temperatura in gradi C (sola lettura): avviso, riduzione, caldo estremo, rientro alla normalita.
void        cpuPmThresholds(float& warn, float& reduce, float& extreme, float& back);
uint32_t    cpuPmLimitMhz();                 // tetto in vigore adesso (prima del primo cambio: la frequenza di avvio)
CpuWhy      cpuPmWhy();                      // motivo del tetto
const char* cpuPmWhyText(CpuWhy w);          // il motivo in parole (tradotto)
const char* cpuPmWhyKey(CpuWhy w);           // "start", "user", "load", "boost", "hot", "https", "sensor"
int         cpuPmStatusJson(char* out, size_t n);   // campi JSON dello stato (cpuMhz...cpuChanges, con la virgola in fondo); byte scritti, -1 se non entra
uint32_t    cpuPmChanges();                  // quanti cambi di tetto dall'avvio
