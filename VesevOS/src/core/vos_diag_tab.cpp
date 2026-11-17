// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_diag_tab.cpp
// La tabella dei codici della diagnostica con i lampi del LED (regole in vos_diag_rule.h). Solo errori che il firmware rileva davvero.
// Un numero tolto non si riusa mai. Nessuna dipendenza dal chip: si prova sul computer (build/tests/diagnostica).
#include "vos_diag_rule.h"

#define TRK(s) s      // testo che si traduce piu tardi (lo scanner delle traduzioni lo trova lo stesso)

const DiagVoce* diagTabella(int* n) {
  static const DiagVoce k[] = {
    {DC_AVVIO,     1, DGR_CRITICO, DQU_PERSISTE, TRK("NVS non parte")},
    {DC_AVVIO,     3, DGR_CRITICO, DQU_PERSISTE, TRK("file system non monta")},
    {DC_CPU,       2, DGR_FATALE,  DQU_AVVIO,    TRK("eccezione della CPU")},
    {DC_CPU,       3, DGR_FATALE,  DQU_AVVIO,    TRK("errore di cache")},
    {DC_CPU,       4, DGR_FATALE,  DQU_AVVIO,    TRK("protezione della memoria")},
    {DC_CPU,       5, DGR_FATALE,  DQU_AVVIO,    TRK("eccezione di debug")},
    {DC_WATCHDOG,  1, DGR_FATALE,  DQU_AVVIO,    TRK("task watchdog")},
    {DC_WATCHDOG,  2, DGR_FATALE,  DQU_AVVIO,    TRK("interrupt watchdog")},
    {DC_WATCHDOG,  3, DGR_FATALE,  DQU_AVVIO,    TRK("altro watchdog")},
    {DC_MEMORIA,   1, DGR_FATALE,  DQU_AVVIO,    TRK("pila di un task esaurita")},
    {DC_ALIM,      1, DGR_CRITICO, DQU_AVVIO,    TRK("tensione bassa (brownout)")},
    {DC_ALIM,      2, DGR_CRITICO, DQU_AVVIO,    TRK("errore di alimentazione o eFuse")},
  };
  if (n) *n = (int)(sizeof(k) / sizeof(k[0]));
  return k;
}

