// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_time.h
// Ora: client NTP, fuso orario con ora legale, server NTP opzionale.
#pragma once
#include <stddef.h>
#include <stdint.h>
#include "vos_errbuf.h"
#include "vos_heapbuf.h"

void   timeInit();                  // avvia il task dell'ora
void   timeApply();                 // rilegge cfg (server, fuso, server NTP)
bool   timeValid();
uint32_t timeLastSync();            // epoch dell'ultima sincronizzazione NTP (0 = mai)                 // l'ora e affidabile?
void   timeNowFmt(char* out, size_t n);  // data e ora locali nei formati scelti ("non impostata" se l'ora non e valida); 32 byte bastano
void   timeSetEpoch(uint32_t t);    // imposta l'ora a mano (dal browser)
bool   timeSetLocal(const char* s, ErrBuf& err);   // "AAAA-MM-GG HH:MM[:SS]" ora locale (fuso della scheda)
bool   timeEveryValid(long minutes);  // valori ammessi per la frequenza NTP
#define TIME_JSON_MAX 768   // tetto di timeJson (i testi della configurazione sono corti)
bool   timeJson(HeapBuf& out);      // oggetto JSON dell'ora (aggiunto in coda)
bool   timePubJson(HeapBuf& out);   // pezzo JSON pubblico per la pagina di accesso (ora, scarto, formati)
void   fmtTemp(float celsius, char* out, size_t n);   // temperatura nell'unita scelta (C o F), es. "24 C", intera; 16 byte bastano
