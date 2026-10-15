// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_wps.h
// WPS: la scheda riceve nome e password della Wi-Fi di casa dal router, senza digitarli.
//  - Metodo a PULSANTE: premi il tasto WPS del router entro 2 minuti.
//  - Metodo PIN (solo se consentito): la scheda crea un PIN a 8 cifre (nuovo a ogni prova, vale 2 minuti) e lo scrivi nel router.
// Parte dalla pagina (solo Admin), dalla shell (solo Admin) o dal tasto BOOT (pressione breve, se consentito).
// Il lavoro con il Wi-Fi lo fa SOLO il task di rete (wpsTick): chi chiede (pagina, tasto) lascia solo una richiesta.
#pragma once
#include <Arduino.h>
#include "../core/vos_common.h"

enum WpsState : uint8_t { WPS_IDLE = 0, WPS_WAIT = 1, WPS_OK = 2, WPS_FAIL = 3 };

void   wpsInit();                          // iscrive il servizio "wps" (da netInit)
bool   wpsAllowed();                       // acceso, oppure prima configurazione non finita
bool   wpsPinAllowed();                    // metodo PIN consentito?
bool   wpsStart(bool pin, const char* who, String& err);   // chiede l'avvio (parte nel task di rete)
void   wpsStop();                          // chiede l'annullamento
bool   wpsActive();                        // richiesta o prova in corso: il task di rete non deve toccare il Wi-Fi
void   wpsAbort();                         // il task di rete deve fare altro: ferma subito, senza ripristinare
void   wpsTick();                          // dal task di rete (ogni secondo)
bool   wpsSetOn(bool on, String& err);     // interruttore del servizio
int    wpsState();
String wpsJson();
String wpsText();
