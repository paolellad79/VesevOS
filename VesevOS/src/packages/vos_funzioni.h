// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_funzioni.h
// Script (linguaggio piccolo, MIT: src/third_party/funzioni). Passo 1: si esegue un TESTO dalla shell (comando "funzioni").
// Ogni esecuzione: un task suo, memoria con tetto (PSRAM), limite di tempo, uscita (print) catturata. Finita l'esecuzione
// l'interprete sparisce e la memoria torna libera. Funzioni offerte allo script: led, act, time, millis, temp, log, sleep, btn, print.
// Lo script NON tocca rete, password, configurazione, file: "act" passa dalle stesse azioni controllate delle Automazioni.
#pragma once
#include "../core/vos_errbuf.h"
#include "../core/vos_common.h"
#include "../core/vos_heapbuf.h"

#define FUNZIONI_STATO_MAX   20480     // tetto del JSON di stato (elenco fino a 200 script + uscita + errore); oltre: errore, mai JSON troncato
#define FUNZIONI_TESTO_MAX    16384     // tetto dei testi della shell (elenco, uscita, stato)

// Argomenti di un comando di driver (vedi vos_funzioni_cmd.cpp): sottocomando e fino a CMD_ARGS_MAX parole, gia controllate da chi chiama.
#define CMD_ARGS_MAX   8
#define CMD_ARG_LEN    96
#define CMD_ARGS_TOTAL 300
struct CmdCall {
  char sub[13];
  int  n;
  char arg[CMD_ARGS_MAX][CMD_ARG_LEN + 1];
};

#if VOS_WITH_FUNZIONI
#ifndef FUNZIONI_MEMORIA_MAX
#define FUNZIONI_MEMORIA_MAX   65536UL    // tetto di memoria per esecuzione (byte)
#endif
#ifndef FUNZIONI_TEMPO_MS
#define FUNZIONI_TEMPO_MS     3000UL     // tempo massimo di un comando "vsh <codice>" (tutto compreso)
#endif
#ifndef FUNZIONI_RAFFICA_MS
#define FUNZIONI_RAFFICA_MS    3000UL     // script in /sbin: tempo massimo SENZA pause (sleep); con sleep puo girare all'infinito
#endif
#define FUNZIONI_CODICE_MAX    1500       // lunghezza massima del testo dello script dato nel comando
#define FUNZIONI_FILE_MAX    8192       // lunghezza massima di uno script in /sbin
#define FUNZIONI_USCITA_MAX     1500       // uscita massima conservata (script in /sbin: si tiene la parte piu recente)
#define FUNZIONI_STACK_DIM  20480      // stack del task (byte, RAM interna, solo mentre lo script gira). 20 KB: il TLS dello Store (scarica https) gira in questo task.
#define FUNZIONI_STACK_USO_MAX 6144     // oltre questo l'esecuzione e fermata (margine per i livelli tra un controllo e l'altro)

bool   funzioniArgomentiOk(const CmdCall& call, ErrBuf& err);   // limiti: massimo 8 parole, 96 caratteri ciascuna, 300 in tutto, solo ASCII stampabile
// Esegue il codice di un comando di driver (sincrono, 3 s, uscita fino a FUNZIONI_USCITA_MAX). use = permessi del driver; admin = diritti di scrittura sulla microSD.
bool   funzioniEseguiComando(const char* code, const char* use, bool admin, const CmdCall& call, HeapBuf& out, ErrBuf& err, uint32_t maxMs = FUNZIONI_TEMPO_MS);   // maxMs: il widget della Home usa un tempo corto

// Esegue il testo. true = finito senza errori. out = uscita di print; err = messaggio d'errore (anche delle funzioni VesevOS, in inglese).
bool   funzioniValuta(const char* code, HeapBuf& out, ErrBuf& err, uint32_t maxMs = FUNZIONI_TEMPO_MS);
void   funzioniTestoStato(HeapBuf& out);   // versione, stato (in esecuzione?), ultima esecuzione (memoria massima, tempo), totali

// Script salvati: /sbin/<nome>.vsb (prima della copia /scripts, vedi core/vos_layout.h) (si scrivono con "write", dalla pagina File o con mkdir/cp; i vecchi /apps/<nome>.be si spostano da soli). Girano in sottofondo, uno alla volta.
bool   funzioniAvvia(const char* name, ErrBuf& err);   // avvia /sbin/<nome>.vsb in sottofondo (torna subito)
bool   funzioniAccoda(const char* name, ErrBuf& err, const char* who);   // da Automazioni/MQTT: parte subito o va in coda (max 4); uno script NON puo avviarne altri
bool   funzioniNomeValido(const char* name);
bool   funzioniAvviaApp(const char* name, const char* code, const char* use, ErrBuf& err, bool appAdmin = false);   // avvia il codice di un'app (permessi = use= del manifest)
bool   funzioniUltimaEApp(const char* name);             // l'ultima esecuzione e di questa app?
bool   funzioniAppInEsecuzione(char* name, size_t n);          // c'e un'app in corso? (name = quale; 32 byte bastano)
bool   funzioniUscitaAppJson(HeapBuf& out);                  // "out":..,"err":..,"ms":..,"last":.. (senza graffe); false = non entra o manca la memoria (out da buttare)
bool   funzioniFerma(ErrBuf& err);                        // ferma lo script (o l'app) in corso (attende fino a ~2,5 s)
bool   funzioniInEsecuzione();
// Il programma in corso adesso (script, app o comando di driver), per l'elenco dei Task. false = non ce n'e nessuno.
struct InfoEsecuzioneFunzioni {
  char     name[25];       // nome dello script o dell'app (vuoto per un comando di driver o un "vsh <codice>")
  bool     app;            // e un'app (.vab)
  bool     cmd;            // e un comando di un driver o un testo dato alla shell: non e un programma dell'utente
  char     use[80];        // permessi dichiarati dall'app (use=, separati da virgola); vuoto per gli script
  uint32_t seconds;        // da quanto gira
  uint32_t memory;         // memoria in uso adesso (byte)
};
bool   funzioniInfoEsecuzione(InfoEsecuzioneFunzioni& r);
bool   funzioniAttive();                                     // sottosistema vdb acceso? (cfg.funzioniAttive)
void   funzioniServizioInit();                            // vos_funzioni_svc.cpp: iscrive il servizio "vdb" (interruttore del sottosistema)
void   funzioniElenca(HeapBuf& out);                       // elenco di /sbin (crea la cartella se manca)
void   funzioniTestoUscita(HeapBuf& out);                    // ultima uscita (print) e ultimo errore dello script in /sbin
bool   funzioniStatoJson(HeapBuf& out);                  // false = non entra o manca la memoria (out da buttare). Per la pagina: {ok, busy, name, bg, list:[{n,s}], out, err, trunc, max, runs} (crea /sbin se manca)
#else
inline bool   funzioniValuta(const char*, HeapBuf&, ErrBuf& err, uint32_t = 0) { err = "Script non presenti in questo firmware"; return false; }
inline void   funzioniTestoStato(HeapBuf& o) { o.add("Script: non presenti in questo firmware"); }
inline bool   funzioniAvvia(const char*, ErrBuf& err) { err = "Script non presenti in questo firmware"; return false; }
inline bool   funzioniAccoda(const char*, ErrBuf& err, const char*) { err = "Script non presenti in questo firmware"; return false; }
inline bool   funzioniNomeValido(const char*) { return false; }
inline bool   funzioniEseguiComando(const char*, const char*, bool, const CmdCall&, HeapBuf&, ErrBuf& err, uint32_t = 0) { err = "Script non presenti in questo firmware"; return false; }
inline bool   funzioniArgomentiOk(const CmdCall&, ErrBuf& err) { err = "Script non presenti in questo firmware"; return false; }
inline bool   funzioniAvviaApp(const char*, const char*, const char*, ErrBuf& err, bool = false) { err = "Script non presenti in questo firmware"; return false; }
inline bool   funzioniUltimaEApp(const char*) { return false; }
inline bool   funzioniAppInEsecuzione(char*, size_t) { return false; }
inline bool   funzioniUscitaAppJson(HeapBuf& o) { return o.add("\"out\":\"\",\"err\":\"\""); }
inline bool   funzioniFerma(ErrBuf& err) { err = "Script non presenti in questo firmware"; return false; }
inline bool   funzioniInEsecuzione() { return false; }
struct InfoEsecuzioneFunzioni { char name[25]; bool app, cmd; char use[80]; uint32_t seconds, memory; };
inline bool   funzioniInfoEsecuzione(InfoEsecuzioneFunzioni&) { return false; }
inline bool   funzioniAttive() { return false; }
inline void   funzioniServizioInit() {}
inline void   funzioniElenca(HeapBuf& o) { o.add("Script: non presenti in questo firmware\n"); }
inline void   funzioniTestoUscita(HeapBuf&) {}
inline bool   funzioniStatoJson(HeapBuf& o) { return o.add("{\"ok\":false,\"err\":\"Script non presenti in questo firmware\"}"); }
#endif
