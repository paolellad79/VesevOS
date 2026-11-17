// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_risorse.h
// Lettore e scrittore delle RISORSE in /usr/share/vesevos: la pagina (file .vrs) e i file singoli (dati comuni, licenze minori, manuale, tabella
// radio) con l'elenco SHA256SUMS. Formato: tools/mkrisorse.py (nessuna ancora al firmware dal 16/11/2026). Qui: la pagina .vrs e il suo caricamento;
// i file singoli sono in vos_risorse_file.h (senza dipendenze dal file system a pezzi, cosi anche vos_region e la shell possono usarli).
// Le regole sono in vos_risorse_rule.h (provate sul computer); qui sono collegate al file system interno e a mbedTLS (SHA-256 con l'acceleratore
// hardware dell IDF, come vos_util). La pagina e servita da /usr/share/vesevos/html.vrs (vos_web_pagina.cpp); gli altri file passano da
// risFileApri (qui sotto), che li controlla con SHA256SUMS la prima volta dopo l'avvio.
#pragma once
#include "vos_risorse_rule.h"
#include "vos_risorse_file.h"
#include "vos_risorse_carica_rule.h"
#include "../drivers/vos_drv_file.h"

// "/usr/share/vesevos/<nome>.vrs" in out. false se il nome non e valido o out e troppo corto.
bool risPercorso(const char* nome, char* out, size_t cap);

// Controllo veloce (pochi byte letti): il file c'e, intestazione e id buoni, nome uguale, dimensione del file = 96 + lunghezza.
// NON controlla lo SHA-256 (vedi risControlla). f resta aperto sul primo byte dei dati: si legge a blocchi con f.read(buf, n).
RisEsito risApri(const char* nome, VosFile& f, RisInfo& info);

// Controllo completo: come risApri e in piu SHA-256 di tutti i dati (legge il file a blocchi da 1 KB, nessuna memoria dinamica).
// Da fare una volta dopo l'avvio e ricordare il risultato; non a ogni richiesta.
RisEsito risControlla(const char* nome, RisInfo* info = nullptr);

// Scrittura ATOMICA di una risorsa che arriva a pezzi: tutto passa da un file temporaneo "/usr/share/vesevos/<nome>.vrs.tmp~" e solo a controlli finiti
// (intestazione, id, tetto, lunghezza, SHA-256) lo scambio lo rende il file vero. Se qualcosa non va o si smette a meta, il file vecchio resta
// com'e. Il nome della risorsa sta nell'intestazione del file che arriva.
struct RisCarico;                                          // un caricamento in corso (nascosto)
RisCarico* risCaricoInizia();                              // nullptr se manca la memoria (circa 700 byte di heap)
RisEsito   risCaricoDai(RisCarico* c, const uint8_t* d, size_t n);
RisEsito   risCaricoFine(RisCarico* c);                    // controlla, scambia e libera SEMPRE c
void       risCaricoAnnulla(RisCarico* c);                 // butta il temporaneo e libera c (nullptr ammesso)

// ---- Caricamento di un file singolo (non la pagina): POST /api/risorse/carica. Il nome viene dall'elenco SHA256SUMS della scheda, mai da chi invia.
// Si accetta solo se lo SHA-256 del contenuto e quello della riga dell'elenco (o, per SHA256SUMS stesso, se l'id e quello del firmware).
// Scrittura atomica: temporaneo + scambio dopo i controlli. Dopo lo scambio lo stato del file si azzera e si ricontrolla.
struct RisFileCarico;
bool            risFileCaricabile(const char* nomeInviato);        // "SHA256SUMS" o un file singolo noto al firmware (non html.vrs)
// esito: RIS_OK, RIS_FORMATO/RIS_MANCA (elenco non buono o nome non nell'elenco), RIS_SPAZIO / RIS_SCRITTURA (memoria o apertura). nullptr se non va.
RisFileCarico*  risFileCaricoInizia(const char* nomeInviato, RisEsito& esito);
RisEsito        risFileCaricoDai(RisFileCarico* c, const uint8_t* d, size_t n);
RisEsito        risFileCaricoFine(RisFileCarico* c);               // controlla, scambia, ricontrolla e libera SEMPRE c
const char*     risFileCaricoNome(const RisFileCarico* c);         // il nome preso dall'elenco
void            risFileCaricoAnnulla(RisFileCarico* c);            // butta il temporaneo e libera c (nullptr ammesso)
