// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_alias.h
// Alias e descrizione di ogni periferica (decisione dell'utente del 13/11/2026), salvati nella configurazione (/etc/vesevos.conf, sezione "alias").
// L'alias e un nome MOSTRATO (menu, titoli, elenchi, schema dei pin): il nome vero (led1, rele2...) resta l'unico identificatore per comandi,
// funzioni VesevOS, regole e API. L'alias NON si accetta mai come bersaglio di un comando.
// Il testo e dell'utente: qui si controlla ogni valore (il firmware non si fida della pagina); chi lo mostra deve comunque fare l'escape.
#pragma once
#include <stddef.h>
#include <stdint.h>
#include <string_view>
#include "vos_heapbuf.h"

#define ALIAS_LEN_MAX    24     // byte dell'alias (una lettera accentata ne vale 2)
#define ALIAS_DESCR_MAX  120    // byte della descrizione
#define ALIAS_NAME_MAX   31     // byte del nome vero della periferica
#define ALIAS_N          32     // periferiche con alias o descrizione (16 a mano + quelle di bordo: 3 al massimo)
#define ALIAS_JSON_MAX   8192   // tetto del JSON delle impostazioni (alias e descrizioni compresi)

// Esito dei controlli. I testi (tradotti) li scrive chi chiama: questo modulo non conosce la lingua, cosi resta fuori dai cicli di vos_i18n.
enum AliasErr : uint8_t { ALIAS_OK = 0, ALIAS_LUNGO, ALIAS_SPAZI, ALIAS_CARATTERI, ALIAS_PIENO, ALIAS_NOME };

// Testo valido? Solo UTF-8 ben formato, senza caratteri di controllo ne < > " \ ne segni che invertono la scrittura, senza spazi ai lati.
// ALIAS_OK = valido (anche vuoto).
AliasErr aliasTestoCheck(const char* testo, size_t maxByte);

// Nome vero di periferica valido come chiave (a-z, 0-9, trattino basso; 1-31 caratteri)
bool aliasNomeOk(const char* nome);

const char* aliasGet(const char* nome);          // "" se non c'e
const char* aliasDescr(const char* nome);        // "" se non c'e

// Imposta alias e descrizione (nullptr = lascia com'e; "" = toglie). Entrambi vuoti = la voce sparisce. ALIAS_OK = fatto; altrimenti valori non validi, nome non valido o tabella piena (nulla cambia).
AliasErr aliasSet(const char* nome, const char* alias, const char* descr);

// Un'altra periferica ha gia questo alias (senza badare alle maiuscole)? Ritorna il nome vero di quella, nullptr se nessuna. Serve solo ad avvisare.
const char* aliasDuplicato(const char* alias, const char* tranne);

bool aliasSposta(const char* da, const char* a);  // rinomina di una periferica: l'alias la segue. true = c'era qualcosa da spostare
bool aliasTogli(const char* nome);                // periferica cancellata. true = c'era qualcosa
void aliasAzzera();                               // valori di fabbrica (cfgDefaults)
int  aliasConta();
const char* aliasNomeAt(int i);                   // nome vero della voce i ("" fuori elenco)

// Configurazione: righe "option <nome>.alias '...'" e "option <nome>.descrizione '...'". Import: chiave intera ("led1.alias"), valore gia senza apici.
bool aliasImportaChiave(std::string_view chiave, std::string_view valore);
void aliasImportaInizio();                        // prima riga della sezione: la lista si sostituisce
// Per l'export: scrive le coppie nome.campo / valore tramite la funzione data
void aliasEsporta(void (*scrivi)(void* ctx, const char* chiave, const char* valore), void* ctx);

// JSON per /api/settings: {"led1":{"a":"...","d":"..."},...}. Il testo UTF-8 passa com'e (HeapBuf::addJson lo trasformerebbe in '?').
bool aliasJson(HeapBuf& out);
