// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_drv_wps.h
// STRATO 1 (driver): unico file che usa il WPS di ESP-IDF (esp_wifi_wps_*) e gli eventi Wi-Fi di Arduino per il WPS.
// Non decide quando partire ne cosa salvare: lo decide il servizio (net/vos_wps).
// Gli eventi arrivano da un altro task: qui si copiano solo in poche variabili, le legge drvWpsPoll() dal task di rete.
#pragma once
#include <Arduino.h>
#include "../boards/vos_packages.h"

enum DrvWpsErr : uint8_t { DWERR_NONE = 0, DWERR_START = 1, DWERR_ABSENT = 2 };   // perche drvWpsStart non parte (il testo lo sceglie il servizio: il driver non traduce)
enum DrvWpsEvent : uint8_t { DWE_NONE = 0, DWE_OK = 1, DWE_FAILED = 2, DWE_TIMEOUT = 3 };

bool        drvWpsAvailable();                 // le librerie della scheda hanno il WPS?
bool        drvWpsStart(bool pin, DrvWpsErr& err);// accende il WPS (pin = false: pulsante del router; true: PIN creato dalla scheda). La scheda deve essere in modo STA o AP+STA
void        drvWpsStop();                      // spegne il WPS (sempre sicuro da chiamare)
DrvWpsEvent drvWpsPoll();                      // ultimo evento nuovo (poi si azzera)
String      drvWpsPin();                       // PIN a 8 cifre (solo dopo drvWpsStart(true) e un attimo di attesa), "" se non c'e
bool        drvWpsCredentials(String& ssid, String& pass);   // nome e password ricevuti dal router (dopo DWE_OK)
