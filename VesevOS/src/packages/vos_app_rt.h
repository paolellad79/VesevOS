// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_app_rt.h
// Memoria dell'app in esecuzione: i valori dei controlli della pagina e dei dati ([data]), la coda dei comandi dalla pagina
// (on_set) e i punti dei grafici. Nessun Berry qui: la parlano il task Berry (vos_berry.cpp) e il server web (via vos_app.cpp).
// Una sola app alla volta. Accesso protetto da un mutex (task Berry e task web).
#pragma once
#include "../core/vos_errbuf.h"
#include "../core/vos_heapbuf.h"
#include "vos_app.h"

#if VOS_WITH_BERRY
#define RT_SLOTS   (APP_CTL_MAX + APP_DATA_MAX)
#define RT_EVQ     8        // comandi dalla pagina in attesa (on_set)
#define RT_TXT     40       // lunghezza massima di un testo

void   rtInit(const AppInfo& a);                    // prepara i valori (controlli + [data]) e svuota la coda. Libera i grafici dell'app precedente.
void   rtFree();                                    // libera i grafici
bool   rtLoaded();
bool   rtNameIs(const char* name);                  // l'app caricata si chiama cosi?

// dal codice Berry (dev_get / dev_set)
bool   rtGet(const char* id, bool& isNum, long& iv, char* sv, size_t n);                 // false = id sconosciuto
bool   rtSetApp(const char* id, bool isNum, long iv, const char* sv, const char** why);  // false = valore non valido (why = motivo)
bool   rtPop(char* id, bool& isNum, long& iv, char* sv, size_t n);                       // prossimo comando dalla pagina (false = coda vuota)

// dalla pagina
bool   rtSetWeb(const char* id, const char* v, ErrBuf& err);                             // valida, aggiorna il valore e mette in coda l'evento
bool   rtValuesJson(HeapBuf& out);                                                       // false = manca la memoria o il tetto (out da buttare). [{"id":..,"n":..} | {"id":..,"s":".."} | {"id":..,"a":[..]}] dei controlli
#endif
