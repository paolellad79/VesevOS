// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_wdset.h
// Cambio dei tempi dei watchdog (pagina, shell, API). Chi chiama deve gia aver controllato che l'utente sia Admin.
// Nessun watchdog si puo spegnere: ogni valore ha un minimo e un massimo (vos_wd_limits.h) e fuori intervallo e rifiutato.
#pragma once
#include "vos_errbuf.h"
#include "vos_wd_limits.h"

// Cambio proposto: -1 = lascia com'e.
struct WdChange {
  int twdt;                 // Task watchdog di ESP-IDF, secondi
  int lim[WD_LIM_N];        // limiti del nostro watchdog, secondi
  WdChange() : twdt(-1) { for (int i = 0; i < WD_LIM_N; i++) lim[i] = -1; }
};

bool wdCheck(const WdChange& c, ErrBuf& err);      // controlla tutti i valori insieme, non cambia nulla
bool wdApply(const WdChange& c, ErrBuf& err);      // controlla, applica subito, salva in config, scrive nel registro
bool wdSetOne(const char* name, long value, ErrBuf& err);   // name = "twdt" oppure il nome di un servizio
bool wdDefaults(ErrBuf& err);                      // valori di base
void wdSetInit();                                  // al boot: crea il blocco e applica il tempo salvato del Task watchdog
