// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_prim.h
// STRATO 6 (packages): REGISTRO UNICO DELLE PRIMITIVE, indipendente dal linguaggio (VOS_WITH_ELF).
// Una primitiva si scrive UNA volta (tipi semplici: int, puntatore + lunghezza, testo) e si collega ai linguaggi:
//   - ELF:   il risolutore (vos_elf_syms.cpp) cerca il nome qui, controlla ruolo, use= e versione API e consegna il puntatore;
//   - Berry: la funzione bf<nome> traduce gli argomenti e chiama la primitiva (collegamento da fare una primitiva alla volta, non ancora).
// Il nome e quello di Berry (log, millis, sleep...). Nel modulo ELF il simbolo e "vos_" + nome (vos_log).
// Ogni gruppo ha il suo file vos_prim_<gruppo>.cpp. Oggi: base (sempre concesso), log, sd (microSD), dev (periferiche gia configurate), gpio, i2c, spi, time e files (la cartella dei dati dell'app).
// Come aggiungerne: docs/progetto/piano-integrazione-elf.md, sezione 4.1. Come migrare le bf* di Berry: sezione 4.2.
// MEMORIA: il modulo non riceve MAI un puntatore del firmware. Dati e testi passano per copia, in buffer del modulo con la lunghezza
// scritta nella chiamata (il firmware non scrive oltre); le risposte sono numeri o testo nel buffer del modulo.
#pragma once
#include <stdint.h>
#include <stddef.h>
#include "../core/vos_common.h"   // VOS_WITH_ELF e i ruoli ROLE_*

#if VOS_WITH_ELF
enum { VOS_PRIM_OK = 0, VOS_PRIM_NO_RUOLO = 1, VOS_PRIM_NO_USE = 2, VOS_PRIM_NO_API = 3, VOS_PRIM_NO_FUNZ = 4 };

struct vos_prim_t {
  const char* nome;        // nome stabile (uguale a Berry)
  const void* fn;          // la funzione (tipi semplici)
  const char* firma;       // firma in C, solo documentazione (e per generare l'header dei moduli)
  uint8_t     ruolo_min;   // ruolo minimo di chi lancia (ROLE_GUEST / ROLE_OPER / ROLE_ADMIN)
  uint16_t    da_api;      // versione dell'API in cui e comparsa
};

#define VOS_PRIM_GRUPPO_BASE "base"   // sempre concesso, senza use=
#define VOS_PRIM_USE_MAX 64           // lunghezza massima di use= (zero compreso) del modulo in esecuzione

// Codici di ritorno delle primitive con tipi semplici: 0 o un numero >= 0 = riuscito; negativo = errore. Uguali per tutti i gruppi.
enum {
  VOS_PRIM_E_ARG = -1,         // argomento nullo, negativo o fuori misura
  VOS_PRIM_E_PERCORSO = -2,    // percorso non valido o fuori dalla microSD
  VOS_PRIM_E_NONTROVATO = -3,  // non c'e, o non lo puoi vedere (file di sistema, chiavi e certificati sono sempre "non trovati")
  VOS_PRIM_E_NOSD = -4,        // microSD non configurata o non montata
  VOS_PRIM_E_VIETATO = -5,     // file di sistema, chiavi, certificati: non si toccano mai
  VOS_PRIM_E_GRANDE = -6,      // blocco oltre il tetto di 1024 byte, o buffer del modulo troppo piccolo per la risposta
  VOS_PRIM_E_IO = -7,          // errore della periferica o del file system
  VOS_PRIM_E_OCCUPATO = -8,    // risorsa in uso (microSD in uso, elenco gia in corso)
  VOS_PRIM_E_USE = -9,         // periferica non concessa a questo modulo (use=)
  VOS_PRIM_E_PIENO = -10       // spazio esaurito (file di dati dell'app)
};

int  vos_prim_registra(const char* gruppo, const vos_prim_t* tab, int n);        // 0 = ok; -1 = registro pieno, nome doppio o argomento nullo
const vos_prim_t* vos_prim_trova(const char* nome, const char** gruppo);         // per nome (come in Berry)
const vos_prim_t* vos_prim_trova_elf(const char* simbolo, const char** gruppo);  // per simbolo del modulo: "vos_" + nome
int  vos_prim_use_ha(const char* use, const char* gruppo);                       // use= "log sd dev": 1 se contiene il gruppo
int  vos_prim_permessa(const char* gruppo, const vos_prim_t* p, int ruolo, const char* use, int api);   // VOS_PRIM_*
int  vos_prim_conta();
// La funzionalita (il gruppo) e stata tolta dall'utente? Il servizio ELF collega qui vos_func.h (funcOn); senza collegamento tutto e concesso.
void vos_prim_funz_set(bool (*concessa)(const char* gruppo));

// Contesto dei moduli IN ESECUZIONE (non quello del carico): uno per task, fino a VOS_PRIM_ESEC_MAX insieme. Lo mette vos_elf_esegui, lo
// leggono le primitive che devono sapere chi le chiama (per esempio dev_get: la periferica deve stare in use=). Le funzioni "esec" valgono
// per il task che le chiama: un modulo non vede mai i permessi di un altro. Chi non e un modulo (shell, pagina) ha ruolo ospite e nessun permesso.
#define VOS_PRIM_ESEC_MAX 6
int  vos_prim_esec_entra(int ruolo, const char* use, const char* nome = "");   // 0 = niente posto, o questo task e gia dentro un modulo; altrimenti il numero del posto (>= 1)
void vos_prim_esec_esce(int posto = 0);                 // posto = quello avuto da entra (anche da un altro task); 0 = il posto di questo task
const char* vos_prim_esec_nome();                       // nome dell'app di questo task ("" se non e un'app: driver o chi non e un modulo)
int  vos_prim_esec_ruolo();                             // ROLE_GUEST se questo task non e un modulo
bool vos_prim_esec_ha(const char* parola);              // la parola e in use= del modulo di questo task (false se non e un modulo)
// Uscita testuale del modulo (comandi e widget dei driver ELF): il chiamante da' un buffer sul posto prima di avviare il modulo, la
// primitiva print() ci scrive (mai oltre cap, taglia in silenzio e segna la riga come troncata). Senza buffer print() scrive nel registro.
void vos_prim_esec_uscita(int posto, char* buf, size_t cap);   // buf = nullptr: nessuna uscita; azzera la lunghezza
size_t vos_prim_esec_scrivi(const char* testo);                // aggiunge al buffer del modulo di questo task; ritorna quanti byte ha scritto (0 se non c'e buffer)
size_t vos_prim_esec_lunghezza(int posto);                     // quanto e stato scritto sul posto
void vos_prim_task_set(void* (*chi)());                        // chi sono (un identificatore per task); lo imposta il servizio ELF. Senza, un task solo

// Testo del modulo -> buffer del firmware, con tetto: false se nullo o senza zero dentro cap byte (mai oltre cap).
bool vos_prim_copia_testo(const char* in, char* out, size_t cap);

void vos_prim_gruppi_registra();     // tutti i gruppi: una volta all'avvio, dal punto di composizione (vos_elf_svc.cpp)
void vos_prim_log_registra();        // gruppi base e log
void vos_prim_sd_registra();         // gruppo sd (solo con VOS_WITH_SD)
void vos_prim_dev_registra();        // gruppo dev
void vos_prim_files_registra();      // gruppo files: la cartella dei dati dell'app (vos_prim_files.cpp)
void vos_prim_hw_registra();         // gruppi gpio, i2c, spi, time (vos_prim_hw.cpp)
#endif
