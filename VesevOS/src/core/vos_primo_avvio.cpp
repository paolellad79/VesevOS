// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_primo_avvio.cpp (vedi vos_primo_avvio.h)
#include "vos_primo_avvio.h"
#include "../drivers/vos_drv_time.h"

static PaCtx g_pa;

void     primoAvvioInit(bool guidaFinita) { paInizia(g_pa, guidaFinita); }
bool     primoAvvioPressione()            { return paPressione(g_pa, drvMillis()) == PAE_APERTA; }
void     primoAvvioFine()                 { paFine(g_pa, drvMillis(), PA_RITARDO_FINE_MS); }
void     primoAvvioAttivita()             { paAttivita(g_pa, drvMillis()); }
PaEvento primoAvvioPasso(bool guidaFinita, int clienti) { return paPasso(g_pa, drvMillis(), guidaFinita, clienti); }
PaStato  primoAvvioStato()                { return g_pa.st; }
bool     primoAvvioAttesa()               { return g_pa.st == PA_ATTESA; }
bool     primoAvvioFinestra()             { return g_pa.st == PA_FINESTRA; }
bool     primoAvvioPendente()             { return g_pa.finitaPendente; }
bool     primoAvvioMinimo()               { return paServiziMinimi(g_pa.st); }
