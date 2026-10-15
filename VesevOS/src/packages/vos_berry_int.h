// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_berry_int.h: stato interno di Berry comune a vos_berry.cpp e vos_berry_scripts.cpp (non usare altrove).
#pragma once
#include "vos_berry.h"

#define BERRY_QMAX    4      // script in coda (azione "script <nome>" delle Automazioni)
#define SCRIPT_DIR    "/scripts"
#define SCRIPT_EXT    ".vsc"
#define BERRY_EBUF_SZ 321    // messaggio d'errore dell'ultima esecuzione

namespace vosberry {
extern portMUX_TYPE g_mux;
extern volatile bool g_busy, g_bg, g_stop, g_app;
extern char     g_obuf[BERRY_OUT_MAX + 1]; extern size_t g_olen; extern bool g_otrunc;
extern char     g_ebuf[BERRY_EBUF_SZ];     extern size_t g_elen;
extern char     g_name[25];
extern uint32_t g_runs, g_lastMs, g_lastPeak, g_lastLeak; extern bool g_lastOk;
extern int g_qn;
extern volatile bool g_toErr;
extern size_t g_used, g_peak;

void qClear();
size_t berryFreeAll();         // in vos_berry_mem.cpp: libera tutto cio che lo script ha lasciato
int  migrateOld();              // sposta i vecchi script .be di /apps in /scripts (una volta per avvio)
}
using namespace vosberry;
