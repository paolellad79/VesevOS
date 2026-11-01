// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_prog.h
// STRATO 6 (packages): i PROGRAMMI dell'utente, tutti insieme: script (.vsb), app Berry (.vab) e app ELF (.vae). Un solo modo di elencare quelli
// in corso (per i Task, "ps" e la pagina), di fermarli, di riavviarli e di avviarli; piu i due SOTTOSISTEMI (vdb = Berry, vde = ELF) con il loro
// interruttore e l'AVVIO AUTOMATICO (programmi che partono da soli dopo i servizi, nell'ordine scelto dall'utente nel pannello Avvio).
// Le estensioni stanno in core/vos_ext.h. Berry esegue un programma alla volta (un solo interprete); l'ELF fino a VOS_ELF_RUN_MAX insieme.
#pragma once
#include <stdint.h>
#include "../core/vos_common.h"
#include "../core/vos_errbuf.h"
#include "../core/vos_heapbuf.h"
#include "vos_prog_kind.h"

#define PROG_USE_MAX 80
#define PROG_RUN_MAX 16
#define PROG_JSON_MAX 4096

struct ProgRun {
  uint8_t  kind;
  char     name[25];
  int      id;               // numero del processo ELF; 0 per Berry
  bool     active;           // in corso (false = finito, solo ELF: gli ultimi quattro)
  uint32_t seconds;          // da quanto gira (o quanto e girato)
  uint32_t memory;           // memoria in uso (Berry) o stimata (ELF), byte
  int      result;           // ELF finito: valore restituito
  uint8_t  how;              // ELF finito: 0 normale, 1 fermato, 2 scaduto
  char     use[PROG_USE_MAX];   // gruppi di primitive concessi ("log sd")
};

const char* progExt(uint8_t kind);                                  // l'estensione (con il punto) di quel tipo
int   progRunning(ProgRun* out, int max, bool conFiniti);           // in corso adesso (e, se richiesto, gli ultimi ELF finiti); ritorna quanti
bool  progStop(const char* nomeOId, ErrBuf& err);                   // ferma il programma in corso con questo nome (o numero ELF)
bool  progStart(uint8_t kind, const char* name, int role, const char* who, ErrBuf& err);   // avvia (per l'ELF: con i gruppi che il modulo chiede)
bool  progRestart(uint8_t kind, const char* name, int role, ErrBuf& err);
void  progPsText(HeapBuf& out);                                     // righe per il comando "ps" (dopo i task del sistema)
bool  progJson(HeapBuf& out);                                       // "subs":[...],"progs":[...] (senza graffe): per /api/tasklist e /api/progs

int   progUsaFunzionalita(const char* id);                          // programmi in corso che usano il gruppo di funzioni (per vos_func.cpp)
void  progFermaFunzionalita(const char* id);                        // ferma quelli che lo usano

// Sottosistemi: vdb (Berry) e vde (ELF). L'interruttore e il servizio nel registro (vos_service): vedi vos_berry_svc.cpp e vos_elf_svc.cpp.
struct ProgSub { const char* id; bool on; int running; uint32_t memory; };
int   progSubs(ProgSub* out, int max);
// Nome accettato nei comandi: "vdb", "vde" (e "elf" come sinonimo di vde). nullptr se non e un sottosistema; altrimenti l'id del servizio.
const char* progSubService(const char* nome);

// Avvio automatico (cfg.boot): i programmi che partono da soli. Le app con autostart=required partono sempre (davanti agli altri), quelle con
// autostart=never non si possono scegliere. Di fabbrica nessuno.
struct ProgBoot { uint8_t kind; char name[25]; int rule; bool forced; };   // rule: 0 permesso, 1 obbligatorio, 2 vietato
bool  progBootJson(HeapBuf& out);                                   // {"list":[{"k":"s","n":..,"r":0,"f":false}],"max":8}
bool  progBootSet(const char* csv, ErrBuf& err);                    // "s:nome,b:nome,e:nome" nell'ordine di avvio; salva in configurazione
void  progBootStart();                                              // una volta all'avvio, dopo i servizi: lancia il task che avvia i programmi
