// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_berry.h
// Script Berry (linguaggio piccolo, MIT: src/third_party/berry). Passo 1: si esegue un TESTO dalla shell (comando "berry").
// Ogni esecuzione: un task suo, memoria con tetto (PSRAM), limite di tempo, uscita (print) catturata. Finita l'esecuzione
// l'interprete sparisce e la memoria torna libera. Funzioni offerte allo script: led, act, time, millis, temp, log, sleep, btn, print.
// Lo script NON tocca rete, password, configurazione, file: "act" passa dalle stesse azioni controllate delle Automazioni.
#pragma once
#include "../core/vos_errbuf.h"
#include "../core/vos_common.h"
#include "../core/vos_heapbuf.h"

#define BERRY_STATE_MAX   20480     // tetto del JSON di stato (elenco fino a 200 script + uscita + errore); oltre: errore, mai JSON troncato
#define BERRY_TEXT_MAX    16384     // tetto dei testi della shell (elenco, uscita, stato)

#if VOS_WITH_BERRY
#ifndef BERRY_MEM_LIMIT
#define BERRY_MEM_LIMIT   65536UL    // tetto di memoria per esecuzione (byte)
#endif
#ifndef BERRY_TIME_MS
#define BERRY_TIME_MS     3000UL     // tempo massimo di un comando "berry <codice>" (tutto compreso)
#endif
#ifndef BERRY_BURST_MS
#define BERRY_BURST_MS    3000UL     // script in /scripts: tempo massimo SENZA pause (sleep); con sleep puo girare all'infinito
#endif
#define BERRY_CODE_MAX    1500       // lunghezza massima del testo dello script dato nel comando
#define BERRY_FILE_MAX    8192       // lunghezza massima di uno script in /scripts
#define BERRY_OUT_MAX     1500       // uscita massima conservata (script in /scripts: si tiene la parte piu recente)
#define BERRY_STACK_SIZE  12288      // stack del task (byte, RAM interna, solo mentre lo script gira)
#define BERRY_STACK_USE_MAX 6144     // oltre questo l'esecuzione e fermata (margine per i livelli tra un controllo e l'altro)

// Esegue il testo. true = finito senza errori. out = uscita di print; err = messaggio d'errore (anche di Berry, in inglese).
bool   berryEval(const char* code, HeapBuf& out, ErrBuf& err, uint32_t maxMs = BERRY_TIME_MS);
void   berryStatusText(HeapBuf& out);   // versione, stato (in esecuzione?), ultima esecuzione (memoria massima, tempo), totali

// Script salvati: /scripts/<nome>.vsc (si scrivono con "write", dalla pagina File o con mkdir/cp; i vecchi /apps/<nome>.be si spostano da soli). Girano in sottofondo, uno alla volta.
bool   berryStart(const char* name, ErrBuf& err);   // avvia /scripts/<nome>.vsc in sottofondo (torna subito)
bool   berryQueue(const char* name, ErrBuf& err, const char* who);   // da Automazioni/MQTT: parte subito o va in coda (max 4); uno script NON puo avviarne altri
bool   berryNameValid(const char* name);
bool   berryStartApp(const char* name, const char* code, const char* use, ErrBuf& err);   // avvia il codice di un'app (permessi = use= del manifest)
bool   berryLastIsApp(const char* name);             // l'ultima esecuzione e di questa app?
bool   berryAppRunning(char* name, size_t n);          // c'e un'app in corso? (name = quale; 32 byte bastano)
bool   berryAppOutJson(HeapBuf& out);                  // "out":..,"err":..,"ms":..,"last":.. (senza graffe); false = non entra o manca la memoria (out da buttare)
bool   berryStop(ErrBuf& err);                        // ferma lo script (o l'app) in corso (attende fino a ~2,5 s)
bool   berryRunning();
void   berryList(HeapBuf& out);                       // elenco di /scripts (crea la cartella se manca)
void   berryOutText(HeapBuf& out);                    // ultima uscita (print) e ultimo errore dello script in /scripts
bool   berryStateJson(HeapBuf& out);                  // false = non entra o manca la memoria (out da buttare). Per la pagina: {ok, busy, name, bg, list:[{n,s}], out, err, trunc, max, runs} (crea /scripts se manca)
#else
inline bool   berryEval(const char*, HeapBuf&, ErrBuf& err, uint32_t = 0) { err = "Berry non presente in questo firmware"; return false; }
inline void   berryStatusText(HeapBuf& o) { o.add("Berry: non presente in questo firmware"); }
inline bool   berryStart(const char*, ErrBuf& err) { err = "Berry non presente in questo firmware"; return false; }
inline bool   berryQueue(const char*, ErrBuf& err, const char*) { err = "Berry non presente in questo firmware"; return false; }
inline bool   berryNameValid(const char*) { return false; }
inline bool   berryStartApp(const char*, const char*, const char*, ErrBuf& err) { err = "Berry non presente in questo firmware"; return false; }
inline bool   berryLastIsApp(const char*) { return false; }
inline bool   berryAppRunning(char*, size_t) { return false; }
inline bool   berryAppOutJson(HeapBuf& o) { return o.add("\"out\":\"\",\"err\":\"\""); }
inline bool   berryStop(ErrBuf& err) { err = "Berry non presente in questo firmware"; return false; }
inline bool   berryRunning() { return false; }
inline void   berryList(HeapBuf& o) { o.add("Berry: non presente in questo firmware\n"); }
inline void   berryOutText(HeapBuf&) {}
inline bool   berryStateJson(HeapBuf& o) { return o.add("{\"ok\":false,\"err\":\"Berry non presente in questo firmware\"}"); }
#endif
