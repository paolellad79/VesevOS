// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_shell_complete.h
// Completamento col tasto Tab, come in bash. Una sola funzione, usata dalla seriale e dalla pagina web (POST /api/shell con tab=1).
#pragma once
#include <stddef.h>
#include <stdint.h>
#include "vos_shell_sess.h"
#include "../drivers/vos_drv_print.h"

#define SHELL_COMPL_MAX   64      // candidati al massimo (oltre, more = true)
#define SHELL_COMPL_POOL  2048    // byte per i nomi dei candidati (oltre, more = true)

// Risultato. Struttura semplice, senza heap: sta in un oggetto statico (seriale) o in un blocco di malloc (pagina).
struct ShellCompletion {
  char     add[112];                       // testo da aggiungere alla riga (puo essere vuoto)
  int      n;                              // quanti candidati
  bool     more;                           // ce ne sono altri che non ci stanno (tetto di numero o di spazio)
  uint16_t off[SHELL_COMPL_MAX];           // dove comincia ogni nome dentro pool, in ordine alfabetico
  char     pool[SHELL_COMPL_POOL];         // i nomi, uno dopo l'altro, chiusi con 0 (le cartelle finiscono con '/')
  void        reset() { add[0] = 0; n = 0; more = false; }
  const char* name(int i) const { return pool + off[i]; }
};

// Completa la riga line (il testo digitato fino al cursore) per chi ha il ruolo role (come in shellExecSess: -1 non entrato) e la sessione sess
// (per la cartella corrente). Cosa completa:
//   - la prima parola: i nomi dei comandi che il RUOLO puo usare (gli altri non compaiono mai). Su riga vuota non fa niente (scelta: niente
//     elenco di tutti i comandi, sono troppi); web = true aggiunge "clear" e "history", che esistono solo nel terminale della pagina;
//   - i percorsi, per i comandi sui file (cd ls cat mkdir rm mv cp write), solo se il ruolo puo usare quel comando: stesso shellResolvePath
//     dei comandi (relativi, assoluti, ~, ..), elenco della cartella con fsListBuild (che nasconde i file protetti), nomi nascosti (.x) solo
//     se la parola comincia con '.', cartelle con '/' finale;
//   - sottocomandi e opzioni noti (tabella in vos_shell_complete_data.cpp, ricavata da help e dai messaggi d'uso dei comandi), filtrati per ruolo.
// Dopo la chiamata: add = caratteri da scrivere (se un solo candidato: anche lo spazio finale, oppure niente per una cartella, che ha gia '/');
// altrimenti il prefisso comune. Se n > 1 e add e vuoto, un secondo Tab di fila elenca i candidati (lo stato "secondo Tab" lo tiene chi chiama).
// room = quanti caratteri si possono ancora aggiungere alla riga: add non li supera mai (se non ci stanno si taglia e lo spazio finale cade).
// Nomi con spazi, virgolette o '\': la shell non li sa scrivere (le parole si dividono sugli spazi, niente escape), quindi non sono mai
// candidati; se la riga contiene un apice o una virgoletta o '\' non si completa niente (le parole non sono quelle che la shell leggerebbe).
// Ritorna true se c'e almeno un candidato. Nessun log di contenuti.
bool shellComplete(uint32_t sess, int role, const char* line, size_t room, bool web, ShellCompletion& out);

// Scrive i candidati in colonne (come bash, 78 colonne), una riga dopo l'altra. Per la seriale.
void shellCompletePrintList(OutSink& o, const ShellCompletion& c);
