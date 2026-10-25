// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_updorder.h
// "Ordine di aggiornamento" per il recovery (contratto: docs/progetto/contratto-aggiornamento-recovery-2026-10-22.md).
// VesevOS NON si aggiorna da solo: scrive un ordine nella NVS (namespace "vosupd"), manda la scheda nel recovery (stesso
// meccanismo di "recovery now") e il recovery lo esegue. Nessuna password passa al recovery. Il riavvio lo fa il chiamante.
//   Modo 1: il recovery scarica da un indirizzo HTTPS e controlla lunghezza e SHA-256 (senza pagina e senza login).
//   Modo 2: il recovery apre la sua pagina e accetta UN caricamento senza login, con un gettone casuale (16 byte) legato all'IP.
// All'avvio dopo l'aggiornamento VesevOS legge l'esito scritto dal recovery, lo mette nel registro e nell'audit, e lo cancella.
#pragma once
#include "vos_errbuf.h"
#include <stddef.h>
#include <stdint.h>

// Esiti del recovery (chiave "res" del contratto)
enum UpdRes : uint8_t { UPD_NONE = 0, UPD_OK, UPD_E_RETE, UPD_E_TLS, UPD_E_SHA, UPD_E_DIM, UPD_E_SCRIVI, UPD_E_SCADUTO, UPD_E_ORDINE, UPD_E_ANNULLATO };

struct UpdOutcome {
  uint8_t res;        // UpdRes; 0 = nessun esito da mostrare
  char    msg[44];    // motivo breve scritto dal recovery (ASCII, inglese): va solo mostrato, mai interpretato
  char    ver[28];    // versione dell'ordine
  bool    fresh;      // letto a questo avvio e non ancora visto da un amministratore
};

// L'indirizzo e consentito? Solo https://, ASCII stampabile, senza credenziali, al massimo 199 caratteri, e deve iniziare con
// uno dei prefissi della lista bianca: quello ufficiale (le Release del progetto su GitHub) piu quelli del file /updurl.txt
// (un prefisso https:// per riga, scritto da un amministratore).
bool updUrlAllowed(const char* url, ErrBuf& err);
// Scrive l'ordine di modo 1 e prepara l'avvio nel recovery. sha = 64 cifre esadecimali (obbligatoria). ver facoltativa. size 0 = non indicata.
bool updOrderNet(const char* url, const char* sha, const char* ver, uint32_t size, ErrBuf& err);
// Scrive l'ordine di modo 2 e prepara l'avvio nel recovery. clientIp = IP dell'amministratore in ordine "a<<24|b<<16|c<<8|d" (come ipOf).
// Se va bene: tokenHex = gettone in 32 cifre esadecimali (da dare SOLO a quell'amministratore, mai nei log).
bool updOrderFile(uint32_t clientIp, char tokenHex[33], ErrBuf& err);

// Esito: updOutcomeBoot() all'avvio (dopo log e audit) legge e cancella le chiavi di esito; il risultato resta in RAM.
void             updOutcomeBoot();
const UpdOutcome& updOutcome();
void             updOutcomeSeen();                  // un amministratore lo ha visto: non e piu "nuovo"
const char*      updResText(int res);               // testo tradotto del motivo (per registro e shell)
