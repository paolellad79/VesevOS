// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_drvwidget.h
// WIDGET della Home definiti dai driver (sezione [widget] dei file /devs/*.vdb e funzione on_widget() del codice [cmd], vedi devices/vos_mod_cmd.h).
// Il firmware non conosce nessun widget di periferica (esempio: la microSD e nel suo driver): disegna soli DATI, la pagina li mostra con esc(), mai script.
//
// COME FUNZIONA
//   - L'elenco dei widget (titolo, icona, testo alternativo, chi lo vede) si legge da /devs alla prima richiesta e dopo ogni caricamento o eliminazione di un driver.
//   - I valori vivi li produce on_widget() in Berry, SENZA diritti di scrittura. Il risultato resta in cache 10 secondi.
//   - La Home non si blocca: on_widget() parte solo se la cache e vecchia E Berry e libero, con un tempo massimo corto (600 ms);
//     se Berry e occupato (un comando, uno script, la pagina del driver aperta) si mostra l'ultimo valore noto. Un comando alla volta (D3).
// Si esclude con VOS_WITH_DRVCMD (boards/vos_packages.h).
#pragma once
#include "../core/vos_common.h"
#include "../core/vos_heapbuf.h"
#include "vos_berry.h"

#define DRVW_JSON_MAX   8192     // tetto del pezzo JSON (2 widget con icona da 2 KB e sequenze di escape)

#if VOS_WITH_DRVCMD && VOS_WITH_BERRY
#define DRVW_MAX        2        // widget di driver nella Home (ognuno tiene ~2,3 KB di heap)
#define DRVW_CACHE_MS   10000UL  // validita dei valori
#define DRVW_RUN_MS     600UL    // tempo massimo di on_widget()
void drvWidgetRefresh();                                  // un driver e stato caricato o tolto: alla prossima richiesta si rilegge
// Aggiunge a out un array JSON  [{"d":nome,"t":titolo,"a":alt,"i":svg,"st":"ok|off","v":testo,"s":testo}, ...]  dei widget che il ruolo puo vedere
// (role: 0 ospite, 1 operatore, 2 admin; gli ospiti non vedono nessun widget). false = manca la memoria o il tetto (out da buttare).
bool drvWidgetsJson(HeapBuf& out, int role);
#else
inline void drvWidgetRefresh() {}
inline bool drvWidgetsJson(HeapBuf& out, int) { return out.add("[]"); }
#endif
