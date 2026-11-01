// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_func.h
// STRATO 6 (packages): le FUNZIONALITA. Sono i gruppi di primitive (vedi vos_prim.h) che l'utente puo TOGLIERE a tutti i programmi, dal pannello
// Sistema > Funzionalita. L'interruttore puo solo togliere: quello che un programma chiede (use= nelle app Berry, i simboli importati nelle
// app ELF) resta la richiesta, e il ruolo di chi lo lancia resta il controllo di sempre. Una funzionalita tolta:
//   - ELF: il modulo che la chiede non si carica (e se girava viene fermato);
//   - Berry: la funzione non esiste piu (gruppi log, time, files, ble, dev) oppure risponde "funzionalita spenta" (gruppi sd, gpio, i2c, spi);
//     un'app che la chiede in use= non parte e, se girava, viene fermata.
// Gli id e i bit nella configurazione sono in core/vos_func_ids.h.
#pragma once
#include "../core/vos_common.h"
#include "../core/vos_errbuf.h"
#include "../core/vos_heapbuf.h"
#include "../core/vos_func_ids.h"

#define FUNC_JSON_MAX 3072

bool        funcOn(const char* id);                    // concessa (non tolta dall'utente)? un id sconosciuto e concesso
bool        funcOnIdx(int i);
const char* funcDesc(int i);                           // una riga, tradotta
int         funcRoleRead(int i);                       // ruolo minimo per le funzioni di lettura (ROLE_*)
int         funcRoleWrite(int i);                      // ruolo minimo per quelle che scrivono o comandano
int         funcUsers(int i);                          // programmi IN CORSO che la usano (app Berry con use=, moduli ELF)
bool        funcSet(int i, bool on, ErrBuf& err);      // salva in configurazione; togliendola ferma i programmi che la usano
bool        funcJson(HeapBuf& out);                    // {"list":[{"id","d","rr","rw","on","u"}]}; false = manca la memoria
// Chi la usa e chi la ferma lo sa vos_prog.cpp (Berry ed ELF): si collega qui una volta all'avvio, cosi questo file non dipende dai linguaggi.
typedef int  (*FuncUsersFn)(const char* id);        // programmi in corso che usano il gruppo
typedef void (*FuncStopFn)(const char* id);         // ferma chi lo usa (e aspetta che sia fermo)
void        funcHooksSet(FuncUsersFn users, FuncStopFn stop);
// Dei gruppi chiesti ("log sd gpio"): il primo che l'utente ha tolto, o nullptr se sono tutti concessi.
const char* funcMissing(const char* use);
