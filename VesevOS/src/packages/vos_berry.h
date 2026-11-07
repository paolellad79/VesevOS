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

// Argomenti di un comando di driver (vedi vos_berry_cmd.cpp): sottocomando e fino a CMD_ARGS_MAX parole, gia controllate da chi chiama.
#define CMD_ARGS_MAX   8
#define CMD_ARG_LEN    96
#define CMD_ARGS_TOTAL 300
struct CmdCall {
  char sub[13];
  int  n;
  char arg[CMD_ARGS_MAX][CMD_ARG_LEN + 1];
};

#if VOS_WITH_BERRY
#ifndef BERRY_MEM_LIMIT
#define BERRY_MEM_LIMIT   65536UL    // tetto di memoria per esecuzione (byte)
#endif
#ifndef BERRY_TIME_MS
#define BERRY_TIME_MS     3000UL     // tempo massimo di un comando "berry <codice>" (tutto compreso)
#endif
#ifndef BERRY_BURST_MS
#define BERRY_BURST_MS    3000UL     // script in /sbin: tempo massimo SENZA pause (sleep); con sleep puo girare all'infinito
#endif
#define BERRY_CODE_MAX    1500       // lunghezza massima del testo dello script dato nel comando
#define BERRY_FILE_MAX    8192       // lunghezza massima di uno script in /sbin
#define BERRY_OUT_MAX     1500       // uscita massima conservata (script in /sbin: si tiene la parte piu recente)
#define BERRY_STACK_SIZE  20480      // stack del task (byte, RAM interna, solo mentre lo script gira). 20 KB: il TLS dello Store (scarica https) gira in questo task.
#define BERRY_STACK_USE_MAX 6144     // oltre questo l'esecuzione e fermata (margine per i livelli tra un controllo e l'altro)

bool   berryCmdArgsOk(const CmdCall& call, ErrBuf& err);   // limiti: massimo 8 parole, 96 caratteri ciascuna, 300 in tutto, solo ASCII stampabile
// Esegue il codice di un comando di driver (sincrono, 3 s, uscita fino a BERRY_OUT_MAX). use = permessi del driver; admin = diritti di scrittura sulla microSD.
bool   berryRunCommand(const char* code, const char* use, bool admin, const CmdCall& call, HeapBuf& out, ErrBuf& err, uint32_t maxMs = BERRY_TIME_MS);   // maxMs: il widget della Home usa un tempo corto

// Esegue il testo. true = finito senza errori. out = uscita di print; err = messaggio d'errore (anche di Berry, in inglese).
bool   berryEval(const char* code, HeapBuf& out, ErrBuf& err, uint32_t maxMs = BERRY_TIME_MS);
void   berryStatusText(HeapBuf& out);   // versione, stato (in esecuzione?), ultima esecuzione (memoria massima, tempo), totali

// Script salvati: /sbin/<nome>.vsb (prima della copia /scripts, vedi core/vos_layout.h) (si scrivono con "write", dalla pagina File o con mkdir/cp; i vecchi /apps/<nome>.be si spostano da soli). Girano in sottofondo, uno alla volta.
bool   berryStart(const char* name, ErrBuf& err);   // avvia /sbin/<nome>.vsb in sottofondo (torna subito)
bool   berryQueue(const char* name, ErrBuf& err, const char* who);   // da Automazioni/MQTT: parte subito o va in coda (max 4); uno script NON puo avviarne altri
bool   berryNameValid(const char* name);
bool   berryStartApp(const char* name, const char* code, const char* use, ErrBuf& err, bool appAdmin = false);   // avvia il codice di un'app (permessi = use= del manifest)
bool   berryLastIsApp(const char* name);             // l'ultima esecuzione e di questa app?
bool   berryAppRunning(char* name, size_t n);          // c'e un'app in corso? (name = quale; 32 byte bastano)
bool   berryAppOutJson(HeapBuf& out);                  // "out":..,"err":..,"ms":..,"last":.. (senza graffe); false = non entra o manca la memoria (out da buttare)
bool   berryStop(ErrBuf& err);                        // ferma lo script (o l'app) in corso (attende fino a ~2,5 s)
bool   berryRunning();
// Il programma Berry in corso adesso (script, app o comando di driver), per l'elenco dei Task. false = non ce n'e nessuno.
struct BerryRunInfo {
  char     name[25];       // nome dello script o dell'app (vuoto per un comando di driver o un "berry <codice>")
  bool     app;            // e un'app (.vab)
  bool     cmd;            // e un comando di un driver o un testo dato alla shell: non e un programma dell'utente
  char     use[80];        // permessi dichiarati dall'app (use=, separati da virgola); vuoto per gli script
  uint32_t seconds;        // da quanto gira
  uint32_t memory;         // memoria in uso adesso (byte)
};
bool   berryRunInfo(BerryRunInfo& r);
bool   berryOn();                                     // sottosistema vdb acceso? (cfg.berryOn)
void   berryServiceInit();                            // vos_berry_svc.cpp: iscrive il servizio "vdb" (interruttore del sottosistema)
void   berryList(HeapBuf& out);                       // elenco di /sbin (crea la cartella se manca)
void   berryOutText(HeapBuf& out);                    // ultima uscita (print) e ultimo errore dello script in /sbin
bool   berryStateJson(HeapBuf& out);                  // false = non entra o manca la memoria (out da buttare). Per la pagina: {ok, busy, name, bg, list:[{n,s}], out, err, trunc, max, runs} (crea /sbin se manca)
#else
inline bool   berryEval(const char*, HeapBuf&, ErrBuf& err, uint32_t = 0) { err = "Script non presenti in questo firmware"; return false; }
inline void   berryStatusText(HeapBuf& o) { o.add("Script: non presenti in questo firmware"); }
inline bool   berryStart(const char*, ErrBuf& err) { err = "Script non presenti in questo firmware"; return false; }
inline bool   berryQueue(const char*, ErrBuf& err, const char*) { err = "Script non presenti in questo firmware"; return false; }
inline bool   berryNameValid(const char*) { return false; }
inline bool   berryRunCommand(const char*, const char*, bool, const CmdCall&, HeapBuf&, ErrBuf& err, uint32_t = 0) { err = "Script non presenti in questo firmware"; return false; }
inline bool   berryCmdArgsOk(const CmdCall&, ErrBuf& err) { err = "Script non presenti in questo firmware"; return false; }
inline bool   berryStartApp(const char*, const char*, const char*, ErrBuf& err, bool = false) { err = "Script non presenti in questo firmware"; return false; }
inline bool   berryLastIsApp(const char*) { return false; }
inline bool   berryAppRunning(char*, size_t) { return false; }
inline bool   berryAppOutJson(HeapBuf& o) { return o.add("\"out\":\"\",\"err\":\"\""); }
inline bool   berryStop(ErrBuf& err) { err = "Script non presenti in questo firmware"; return false; }
inline bool   berryRunning() { return false; }
struct BerryRunInfo { char name[25]; bool app, cmd; char use[80]; uint32_t seconds, memory; };
inline bool   berryRunInfo(BerryRunInfo&) { return false; }
inline bool   berryOn() { return false; }
inline void   berryServiceInit() {}
inline void   berryList(HeapBuf& o) { o.add("Script: non presenti in questo firmware\n"); }
inline void   berryOutText(HeapBuf&) {}
inline bool   berryStateJson(HeapBuf& o) { return o.add("{\"ok\":false,\"err\":\"Script non presenti in questo firmware\"}"); }
#endif
