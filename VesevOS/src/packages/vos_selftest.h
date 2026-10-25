// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_selftest.h
// Autodiagnosi: prove non distruttive una per volta, esito di ognuna e report di testo.
// Il report non contiene mai password, chiavi o token; MAC, nome Wi-Fi, IP e log sono oscurati (salvo "nomi reali").
// Nulla viene inviato fuori: il report resta in RAM finche non si chiude o si riavvia.
#pragma once
#include "../core/vos_errbuf.h"
#include "../core/vos_heapbuf.h"
#include "../drivers/vos_drv_string.h"
#include <string>

enum SelfRes : uint8_t { SELF_PEND = 0, SELF_OK, SELF_WARN, SELF_ERR, SELF_SKIP };

bool   selftestStart(bool active, bool names, ErrBuf& err);   // active = prove attive (LED, MQTT); names = nomi reali nel report
bool   selftestRunning();
int    selftestDone();                  // prove finite finora
int    selftestTotal();
std::string selftestLine(int i);           // una riga di testo per la shell: "[OK] Nome: dettaglio" ("" se non ancora pronta)
#define SELFTEST_JSON_MAX   14336
#define SELFTEST_REPORT_MAX 16384
bool   selftestJson(HeapBuf& out);      // stato + risultati finora; false = manca la memoria o supera il tetto
bool   selftestReport(HeapBuf& out);    // report di testo completo; false = non c'e (o manca la memoria)
void   selftestClear();                 // libera la memoria del report
