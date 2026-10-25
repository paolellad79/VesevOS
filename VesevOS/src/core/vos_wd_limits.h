// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_wd_limits.h
// Valori di base e intervalli ammessi dei watchdog, senza dipendenze (cosi si provano sul computer).
// Regola: un watchdog non si puo spegnere. Ogni valore ha un minimo (sotto il quale scatterebbero i falsi allarmi)
// e un massimo (sopra il quale un blocco vero non verrebbe piu visto in tempo utile). Fuori intervallo = rifiutato.
#pragma once
#include <stdint.h>
#include <string.h>

// Task watchdog di ESP-IDF (TWDT), in secondi. Prima 5 s; ora 30 s: un'operazione lunga e lecita (file system, TLS)
// non fa piu riavviare la scheda. Minimo 5 (il vecchio valore), massimo 60 (oltre, un blocco dura troppo).
static const uint8_t WD_TWDT_DEF = 30, WD_TWDT_MIN = 5, WD_TWDT_MAX = 60;

// Watchdog di interruzione (INT_WDT): solo compilazione, in pagina e in sola lettura. Cambiarlo a caldo non e possibile.
static const uint16_t WD_INT_MS = 1000;

// Limiti del nostro watchdog software, uno per servizio controllato (secondi senza segno di vita).
// Il monitor durante l'aggiornamento dello spazio file usa un limite fisso piu largo (WD_FS_REFRESH_SEC).
enum { WD_LIM_NET, WD_LIM_MONITOR, WD_LIM_MESH, WD_LIM_MQTT, WD_LIM_LED, WD_LIM_N };
struct WdLimit { const char* name; uint16_t def, min, max; };
static const WdLimit WD_LIMITS[WD_LIM_N] = {
  {"net",     180,  60, 600},   // tryClient puo durare 20 s e la scansione qualche secondo: sotto 60 s i falsi allarmi
  {"monitor",  60,  30, 600},   // giro di 1 s, ma letture lente (sensori, file): sotto 30 s i falsi allarmi
  {"mesh",     90,  30, 600},   // attese radio ESP-NOW e invii ripetuti
  {"mqtt",     90,  30, 600},   // connessione al broker e TLS possono durare decine di secondi
  {"led",      30,  10, 300},   // giro velocissimo; il limite largo serve solo contro i blocchi veri
};
static const uint16_t WD_FS_REFRESH_SEC = 300;   // monitor durante drvFsUsedRefresh (scorre tutto il file system)

static inline int wdLimIndex(const char* name) {
  if (!name) return -1;
  for (int i = 0; i < WD_LIM_N; i++) if (!strcmp(WD_LIMITS[i].name, name)) return i;
  return -1;
}
static inline bool wdLimOk(int i, long v) { return i >= 0 && i < WD_LIM_N && v >= WD_LIMITS[i].min && v <= WD_LIMITS[i].max; }
static inline bool wdTwdtOk(long s) { return s >= WD_TWDT_MIN && s <= WD_TWDT_MAX; }
