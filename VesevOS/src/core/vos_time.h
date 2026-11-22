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
void   timeNowFmt(char* out, size_t n);  // data e ora locali, formato corto ("non impostata" se l'ora non e valida); TIME_FMT_MAX byte
void   timeSetEpoch(uint32_t t);    // imposta l'ora a mano (dal browser)
bool   timeSetLocal(const char* s, ErrBuf& err);   // "AAAA-MM-GG HH:MM[:SS]" ora locale (fuso della scheda)
bool   timeEveryValid(long minutes);  // valori ammessi per la frequenza NTP
#define TIME_JSON_MAX 768   // tetto di timeJson (i testi della configurazione sono corti)
bool   timeJson(HeapBuf& out);      // oggetto JSON dell'ora (aggiunto in coda)
bool   timePubJson(HeapBuf& out);   // pezzo JSON pubblico per la pagina di accesso (ora, scarto, formati)
// Ora e data da un'epoca (secondi dal 1970), come in un foglio di calcolo: il dato resta un numero e i formati scelti in
// Localizzazione (data corta e lunga, ora, fuso) si applicano solo qui, al momento di mostrarlo. ep = 0: scrive "-".
// Il risultato e tagliato, mai fuori dal buffer: TIME_FMT_MAX byte bastano per i formati pronti e quasi sempre per quelli scritti a mano.
#define TIME_FMT_MAX 64
// CORTA: data e ora con ore e minuti, mai secondi (tabelle, registro, diario, elenchi, registratore).
void   timeFmtEpoch(uint32_t ep, bool conData, char* out, size_t n);
// LUNGA: data lunga e ora lunga (riquadri grandi).
void   timeFmtEpochLungo(uint32_t ep, bool conData, char* out, size_t n);
// Orario programmato (minuti dalla mezzanotte), ora corta: "7:30" oppure "7:30 AM". 12 byte bastano.
void   timeFmtMinutes(unsigned minutes, char* out, size_t n);
// Controllo dei formati scritti a mano (vedi vos_datefmt.h): vero se tutti vanno bene, altrimenti scrive il motivo in err.
bool   timeFmtControlla(int dataCorta, const char* testoCorta, int dataLunga, const char* testoLunga, int oraLunga, const char* testoOra, ErrBuf& err);
const char* timeFmtMessaggio(int errore);   // testo di un DfErr
void   fmtTemp(float celsius, char* out, size_t n);   // temperatura nell'unita scelta (C o F), es. "24 C", intera; 16 byte bastano
