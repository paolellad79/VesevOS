// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_mod_app.h
// Driver completo (.vdb con [page] [code] [data]): il file in /devs e l'UNICO contenitore (icona, schema, pagina, codice, permessi).
// Quando si installa, la sua parte "app" viene scritta in /bin/<nome>.vab e gira come una app normale (stessi permessi use=, stesso controllo,
// nessun diritto in piu). Il file generato comincia con un commento di riconoscimento: un'app scritta a mano con lo stesso nome non si tocca mai.
#pragma once
#include <string_view>
#include "../core/vos_errbuf.h"
#include "../core/vos_heapbuf.h"

bool modAppCheck(const char* name, std::string_view vdp, ErrBuf& err);          // true se il driver non ha [code] oppure pagina e codice sono validi come app
bool modAppInstall(const char* name, ErrBuf& err);                               // scrive /bin/<nome>.vab dal driver salvato; true anche senza [code]
void modAppRemove(const char* name);                                             // toglie /bin/<nome>.vab solo se l'ha generato un driver
// Installazione completa: app del driver, poi periferiche sui pin scelti (pinsCsv) o proposti; se le periferiche falliscono l'app si toglie.
bool modInstallDriver(const char* name, const char* prefix, const char* pinsCsv, HeapBuf& outJson, ErrBuf& err);
bool modSaveDriver(const char* nomeCaricato, std::string_view text, ErrBuf& err, char* salvato = nullptr, size_t cap = 0, ErrBuf* avviso = nullptr);        // controlla (anche l'app) e salva /devs/<nome>.vdb
bool modDeleteDriver(const char* name, ErrBuf& err);                             // toglie il driver e la sua app; le periferiche gia create restano
