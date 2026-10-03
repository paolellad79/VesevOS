// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// VesevOS - vos_time.h
// Ora: client NTP, fuso orario con ora legale, server NTP opzionale.
#pragma once
#include <Arduino.h>

void   timeInit();                  // avvia il task dell'ora
void   timeApply();                 // rilegge cfg (server, fuso, server NTP)
bool   timeValid();                 // l'ora e affidabile?
String timeNowStr();                // "AAAA-MM-GG HH:MM:SS" (ora locale)
void   timeSetEpoch(uint32_t t);    // imposta l'ora a mano (dal browser)
String timeJson();                  // stato per la pagina
String fmtTemp(float celsius);        // temperatura nell'unita scelta (C o F)
