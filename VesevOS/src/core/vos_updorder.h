// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_updorder.h
// "Ordine di aggiornamento" per il recovery (contratto: docs/progetto/contratto-aggiornamento-recovery-2026-10-22.md).
// VesevOS NON si aggiorna da solo: scrive un ordine nella NVS (namespace "vosupd"), manda la scheda nel recovery (stesso
// meccanismo di "recovery now") e il recovery lo esegue. Nessuna password passa al recovery. Il riavvio lo fa il chiamante.
//   Modo 1: il recovery scarica da un indirizzo HTTPS e controlla lunghezza e SHA-256 (senza pagina e senza login).
//   Modo 3: il recovery legge dalla microSD il file gia scaricato da VesevOS, senza rete.
//   Modo 2: il recovery apre la sua pagina e accetta UN caricamento senza login, con un gettone casuale (16 byte) legato all'IP.
// All'avvio dopo l'aggiornamento VesevOS legge l'esito scritto dal recovery, lo mette nel registro e nell'audit, e lo cancella.
#pragma once
#include "vos_errbuf.h"
#include <stddef.h>
#include <stdint.h>

// Esiti del recovery (chiave "res" del contratto)
enum UpdRes : uint8_t { UPD_NONE = 0, UPD_OK, UPD_E_RETE, UPD_E_TLS, UPD_E_SHA, UPD_E_DIM, UPD_E_SCRIVI, UPD_E_SCADUTO, UPD_E_ORDINE, UPD_E_ANNULLATO, UPD_E_RIPRISTINO };

struct UpdOutcome {
  uint8_t res;        // UpdRes; 0 = nessun esito da mostrare
  char    msg[44];    // motivo breve scritto dal recovery (ASCII, inglese): va solo mostrato, mai interpretato
  char    ver[28];    // versione dell'ordine
  bool    fresh;      // letto a questo avvio e non ancora visto da un amministratore
};

// L'indirizzo e consentito? Solo https://, ASCII stampabile, senza credenziali, al massimo 199 caratteri, e deve iniziare con
// uno dei prefissi della lista bianca: quelli ufficiali (le Release del progetto su GitHub e la cartella firmware/ del repository)
// piu quelli del file /updurl.txt (un prefisso https:// per riga, scritto da un amministratore).
bool updUrlAllowed(const char* url, ErrBuf& err);
bool updUrlUfficiale(const char* url);              // solo i prefissi ufficiali: l'installazione automatica non accetta altro
// Scrive l'ordine di modo 1 e prepara l'avvio nel recovery. sha = 64 cifre esadecimali (obbligatoria). ver facoltativa. size 0 = non indicata.
// automatico = installazione senza nessuno davanti: se non riesce il recovery 0.5.3+ torna da solo a VesevOS (app0 permettendo).
bool updOrderNet(const char* url, const char* sha, const char* ver, uint32_t size, ErrBuf& err, bool automatico = false);
// Pin ammessi per la microSD nel recovery: niente flash (26-32), niente USB (19, 20), niente doppioni.
// Stessa regola in recovery/VesevOS_Recovery_IDF/rec_order.cpp (sdPinsOk), progetto a parte: se cambi qui, cambia anche li.
inline bool updPinSdOk(const int pin[4]) {
  for (int i = 0; i < 4; i++) {
    if (pin[i] < 0 || pin[i] > 48 || pin[i] == 19 || pin[i] == 20 || (pin[i] >= 26 && pin[i] <= 32)) return false;
    for (int j = 0; j < i; j++) if (pin[i] == pin[j]) return false;
  }
  return true;
}
// Ordine di modo 3: il file e gia sulla microSD ed e stato controllato. file = percorso sulla scheda ("/AGG/FW.BIN").
// pin = CS, MOSI, SCK, MISO. E sempre automatico: va bene solo dal recovery 0.5.3.
bool updOrderSd(const char* file, const char* sha, const char* ver, uint32_t size, const int pin[4], uint32_t khz, ErrBuf& err);
// Copia di riserva del firmware in uso, sulla microSD (spazio NVS "vosris", contratto sez. 3 quater). Si arma prima dell'ordine di modo 3.
bool updRiservaArma(const char* file, const char* sha, uint32_t size, const char* verNuova, const int pin[4], uint32_t khz, ErrBuf& err);
void updRiservaDisarma();
// Scrive l'ordine di modo 2 e prepara l'avvio nel recovery. clientIp = IP dell'amministratore in ordine "a<<24|b<<16|c<<8|d" (come ipOf).
// Se va bene: tokenHex = gettone in 32 cifre esadecimali (da dare SOLO a quell'amministratore, mai nei log).
bool updOrderFile(uint32_t clientIp, char tokenHex[33], ErrBuf& err);

// Esito: updOutcomeBoot() all'avvio (dopo log e audit) legge e cancella le chiavi di esito; il risultato resta in RAM.
void             updOutcomeBoot();
const UpdOutcome& updOutcome();
void             updOutcomeSeen();                  // un amministratore lo ha visto: non e piu "nuovo"
const char*      updResText(int res);               // testo tradotto del motivo (per registro e shell)
