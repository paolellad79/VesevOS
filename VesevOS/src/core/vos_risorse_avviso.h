// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_risorse_avviso.h
// Controllo di TUTTI i file di /usr/share/vesevos dopo l'avvio, a pezzi, e avviso se uno e diverso dall'elenco SHA256SUMS o manca.
// Lo stato di ogni file e quello di vos_risorse_file.h (un posto solo). Avvisi: registro (vlog), audit e i dati per pagina e shell.
#pragma once
#include <stddef.h>
#include "vos_risorse_file.h"
#include "vos_heapbuf.h"

void risorseTick();                          // dal ciclo principale: un pezzo da 1 KB per volta, mai bloccante; parte dopo qualche secondo dall'avvio

// Primo file con problema (diverso o mancante; "SHA256SUMS" se l'elenco non e valido) e quanti sono. false = nessun problema.
bool risorseProblema(char* nome, size_t cap, RisStato& stato, int& quanti);

// {"rid":"<id firmware>","el":0|1|2,"f":[{"n":"nome","s":0..3,"h":"<sha256 della riga>","t":<byte al massimo>}...]}
// el: 0 elenco buono, 1 manca, 2 non valido. s: RisStato. h solo se l'elenco e buono. Per GET /api/aggiorna.
bool risorseJson(HeapBuf& j);

// {"k":quanti,"n":"nome","s":stato} per /api/status (solo Amministratore): la campanella della pagina. k = 0: niente da segnalare.
bool risorseNotaJson(HeapBuf& j);
