// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_time.h
// Ora: client NTP, fuso orario con ora legale, server NTP opzionale.
#pragma once
#include <Arduino.h>

void   timeInit();                  // avvia il task dell'ora
void   timeApply();                 // rilegge cfg (server, fuso, server NTP)
bool   timeValid();
uint32_t timeLastSync();            // epoch dell'ultima sincronizzazione NTP (0 = mai)                 // l'ora e affidabile?
String timeNowStr();                // "AAAA-MM-GG HH:MM:SS" (ora locale)
void   timeSetEpoch(uint32_t t);    // imposta l'ora a mano (dal browser)
bool   timeSetLocal(const String& s, String& err);   // "AAAA-MM-GG HH:MM[:SS]" ora locale (fuso della scheda)
bool   timeEveryValid(long minutes);  // valori ammessi per la frequenza NTP
String timeJson();
String timePubJson();               // pezzo JSON pubblico per la pagina di accesso (ora, scarto, formati)
String fmtTemp(float celsius);        // temperatura nell'unita scelta (C o F)
