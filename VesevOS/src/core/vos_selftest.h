// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_selftest.h
// Autodiagnosi: prove non distruttive una per volta, esito di ognuna e report di testo.
// Il report non contiene mai password, chiavi o token; MAC, nome Wi-Fi, IP e log sono oscurati (salvo "nomi reali").
// Nulla viene inviato fuori: il report resta in RAM finche non si chiude o si riavvia.
#pragma once
#include <Arduino.h>

enum SelfRes : uint8_t { SELF_PEND = 0, SELF_OK, SELF_WARN, SELF_ERR, SELF_SKIP };

bool   selftestStart(bool active, bool names, String& err);   // active = prove attive (LED, MQTT); names = nomi reali nel report
bool   selftestRunning();
int    selftestDone();                  // prove finite finora
int    selftestTotal();
String selftestLine(int i);             // una riga di testo per la shell: "[OK] Nome: dettaglio" ("" se non ancora pronta)
String selftestJson();                  // stato + risultati finora
String selftestReport();                // report di testo completo ("" se non c'e)
void   selftestClear();                 // libera la memoria del report
