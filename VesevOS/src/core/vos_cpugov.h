// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_cpugov.h
// Governor della CPU: scelgono il TETTO fra 80, 160 e 240 MHz in base al carico. Il lavoro fine (80 a riposo, tetto sotto carico)
// lo fa il gestore di energia di ESP-IDF (drivers/vos_drv_cpu.*).
// Funzione pura: nessun accesso all'hardware, si prova sul computer (tools/hosttest/test_cpugov.cpp).
// Scritti da zero dal comportamento descritto in docs/progetto/piano-governor-cpu-2026-10-22.md (nessun codice di terzi).
// Le soglie sono PROVVISORIE: vanno tarate sulla scheda.
#pragma once
#include <stdint.h>

enum CpuGov : uint8_t { GOV_ONDEMAND = 0, GOV_CONSERVATIVE = 1, GOV_SCHEDUTIL = 2 };

// Motivo dell'ultima decisione del governor.
enum CpuGovWhy : uint8_t { GW_NONE = 0, GW_UP = 1, GW_DOWN = 2, GW_BOOST = 3 };

// Parametri (si cambiano da shell e pagina, solo Admin).
struct CpuGovParams {
  uint8_t  up;       // soglia di salita, % di occupazione (50..95)
  uint8_t  down;     // soglia di discesa, % (5..50, sempre piu bassa di up - 10)
  uint16_t hold;     // secondi minimi sotto soglia prima di scendere (2..120)
};
static const CpuGovParams CPUGOV_DEFAULTS = {80, 20, 10};

// Stato fra un campione e il successivo (parte tutto a zero).
struct CpuGovState {
  uint32_t cap;          // tetto scelto (0 = ancora nessuna decisione)
  uint32_t lastChange;   // secondo dell'ultimo cambio di tetto
  uint32_t belowSince;   // secondo da cui il carico chiede meno del tetto (se below)
  uint32_t peak;         // il piu alto fra i livelli chiesti mentre scende
  bool     below;
  CpuGovWhy why;
};

// Misure di questo secondo.
struct CpuGovInput {
  uint32_t busyPct;      // % occupata del core piu carico (0..100), misurata mentre la CPU andava a curMhz
  uint32_t curMhz;       // tetto in vigore nel secondo appena passato (80, 160 o 240)
  bool     boost;        // c'e una prenotazione attiva (cpuBoost)
  uint32_t nowSec;       // contatore dei secondi (sale di 1 a ogni campione)
};

// Un solo cambio di tetto ogni tanti secondi (la salita da prenotazione fa eccezione).
static const uint32_t CPUGOV_MIN_GAP_SEC = 2;

bool     cpuGovParamsValid(int up, int down, int hold);
// Campione: aggiorna lo stato e restituisce il tetto (80, 160 o 240).
uint32_t cpuGovDecide(CpuGov gov, const CpuGovParams& p, CpuGovState& st, const CpuGovInput& in);
// Carico riportato a 240 MHz equivalenti (%): lavoro svolto, non tempo occupato.
uint32_t cpuGovEquiv240(uint32_t busyPct, uint32_t curMhz);
const char* cpuGovName(CpuGov gov);                    // "ondemand", ...
bool     cpuGovParse(const char* text, CpuGov& gov);   // false = nome non ammesso
