// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_prog_data.h
// STRATO 6 (packages): DOVE le app tengono i propri dati. Decisione dell'utente del 31/10/2026 (docs/progetto/decisioni-sd-file-app-2026-10-31.md):
//   - dove salva un'app lo sceglie l'UTENTE (non l'app): memoria flash oppure microSD, dalla pagina App, solo per le app che salvano dati
//     (app Berry con data=1 nel manifest; app ELF che importano le primitive vos_data_*);
//   - cartelle: flash /var/lib/<nome>/ (come Debian), microSD /media/sd/app/<nome>/; senza microSD la scelta e la flash;
//   - se la microSD sparisce mentre l'app e configurata su SD, l'app si FERMA (registro, allarme, evento): niente ritorno alla flash da sola;
//   - l'app vede e tocca SOLO la sua cartella: file con nome semplice (a-z A-Z 0-9 _ . -), nessun sotto-percorso (primitive data_*).
// Le primitive (Berry: vos_berry_data.cpp; ELF: vos_prim_files.cpp) sono sottili: il lavoro vero e qui, uguale per tutti i linguaggi.
// Questo modulo non conosce ne le app ne i programmi in corso (sarebbe un ciclo): chi controlla e chi ferma sta in vos_prog_where.h.
#pragma once
#include <stdint.h>
#include <stddef.h>
#include "vos_prog_kind.h"
#include "../core/vos_errbuf.h"
#include "../core/vos_heapbuf.h"

enum ProgWhere : uint8_t { PW_FLASH = 0, PW_SD = 1 };

// Codici di ritorno delle operazioni sui file di dati: >= 0 riuscito (byte), negativo = errore. Gli stessi valori di VOS_PRIM_E_* (vos_prim.h).
enum {
  PD_E_ARG = -1,          // argomento nullo o fuori misura
  PD_E_PERCORSO = -2,     // nome di file non ammesso
  PD_E_NONTROVATO = -3,   // il file non c'e
  PD_E_NOSD = -4,         // la microSD scelta non e montata (o si sta formattando)
  PD_E_VIETATO = -5,      // l'app non salva dati (data=1) o non e un'app
  PD_E_GRANDE = -6,       // blocco oltre PD_IO_MAX
  PD_E_IO = -7,           // errore del file system
  PD_E_PIENO = -10        // spazio esaurito
};
#define PD_IO_MAX   1024  // byte al massimo per chiamata
#define PD_FILE_MAX 40    // caratteri di un nome di file

int   progWhere(uint8_t kind, const char* name);                     // PW_SD solo se l'utente l'ha scelta, altrimenti PW_FLASH
bool  progSdUsable();                                                // la microSD e montata e non si sta formattando?
bool  progWhereStore(uint8_t kind, const char* name, int where, ErrBuf& err);   // salva la scelta in configurazione (i controlli "salva dati" e "non in corso" sono di vos_prog_where.h)
bool  progDataReady(uint8_t kind, const char* name, ErrBuf& err);    // prima di avviare: scelta SD e scheda assente = no
bool  progDataDir(uint8_t kind, const char* name, char* out, size_t cap, ErrBuf& err);   // la cartella dati (la crea): out "/var/lib/<nome>" o "/media/sd/app/<nome>"
int   progDataWrite(uint8_t kind, const char* name, const char* file, const uint8_t* buf, size_t n, bool append);   // byte scritti o PD_E_*
int   progDataRead(uint8_t kind, const char* name, const char* file, uint32_t off, uint8_t* buf, size_t n);         // byte letti (0 = oltre la fine) o PD_E_*
int   progDataRemove(uint8_t kind, const char* name, const char* file);                                               // 0 o PD_E_*
bool  progDataJson(HeapBuf& out);                                    // {"sd":true,"list":[{"k":"b","n":"nome"}]}: se la microSD e utilizzabile e le app su SD
