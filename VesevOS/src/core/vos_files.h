// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_files.h
// Gestione file e cartelle sulla memoria interna (LittleFS).
// Il file di configurazione (con le password) e protetto: non si vede e non si tocca.
// I percorsi si puliscono in buffer fissi; niente String.
#pragma once
#include "vos_errbuf.h"
#include <stdint.h>
#include <string_view>
#include "../drivers/vos_drv_string.h"
#include "vos_heapbuf.h"

class VosMutex;

#define FS_PATH_MAX 100              // caratteri di un percorso pulito (zero escluso)
#define FS_DIR_MAX  20               // voci (file e sottocartelle) massime in una cartella: oltre si rifiuta, con un errore chiaro
#define FS_JSON_MAX 40960            // tetto dell'elenco di una cartella: oltre si ferma e dice more:true
#define FS_DIRS_MAX 16384            // tetto dell'elenco di tutte le cartelle (64 cartelle di 100 caratteri ci stanno)
#define FS_TMP_SCAN_MAX (2 * FS_DIR_MAX)   // pulizia dei .tmp~: voci guardate in una cartella (oltre ci si ferma: ogni passo costa tempo)
#define FS_TMP_CAND_MAX 8                  // pulizia dei .tmp~: temporanei trattati per cartella in un giro (gli altri al prossimo avvio)

// Percorso pulito in out (almeno FS_PATH_MAX + 1 byte). Toglie gli spazi ai lati e la barra finale; un percorso vuoto diventa "/".
// false se non e valido: piu di 100 caratteri, "..", "//", "\", caratteri di controllo (anche uno zero dentro) o non ASCII.
bool   fsCleanTo(std::string_view in, char* out, size_t cap);
bool   fsProtected(const char* cleanPath);       // file di sistema nascosti
// Elenco {"ok":true,"path":"/","list":[{n,d,s}],"more":false,...} scritto in j (che ha il tetto FS_JSON_MAX).
// Un percorso non valido o una cartella che manca danno un JSON {"ok":false,"err":...} e ritornano true: false solo se manca la memoria.
bool   fsListBuild(std::string_view path, HeapBuf& j);
const char* fsRoomFor(std::string_view path);          // si puo creare questa voce? nullptr = si (anche se esiste gia: si sovrascrive); altrimenti il testo dell'errore
bool   fsMkdir(std::string_view path, ErrBuf& err);
bool   fsRemove(std::string_view path, ErrBuf& err);   // anche cartelle (con contenuto)
bool   fsRename(std::string_view from, std::string_view to, ErrBuf& err);
bool   fsCopy(std::string_view from, std::string_view to, ErrBuf& err);
bool   fsWriteText(std::string_view path, std::string_view text, ErrBuf& err, bool mustBeNew = false);   // scrittura sicura
bool   fsReadText(std::string_view path, HeapBuf& out, ErrBuf& err);   // max 32 KB, UTF-8, no binari; out ha il suo tetto (almeno 32 KB)
bool   fsDirsBuild(HeapBuf& j);                  // ["/","/lang",...] tutte le cartelle (tetto FS_DIRS_MAX); false solo se manca la memoria
size_t fsTextMax();                              // limite dell'editor in byte
VosMutex& fsSaveMutex();                         // il blocco dei salvataggi: chi scrive un temporaneo "X.tmp~" lo tiene fino allo scambio (vedi vos_files_stream.h)

// Pulizia dei temporanei "nome.tmp~" rimasti da un taglio di corrente tra la scrittura e lo scambio di fsWriteText.
// Regola: si cancella "X.tmp~" SOLO se esiste anche il file "X" (il vecchio contenuto e valido: lo scambio non e mai avvenuto). Se "X" manca il
// temporaneo puo essere l'unica copia: si lascia e si segnala. I file protetti non si toccano. Non si scende nelle sottocartelle.
struct FsTmpStat {
  uint16_t scanned = 0;                   // voci guardate
  uint16_t removed = 0;                   // temporanei cancellati
  uint16_t orphan = 0;                    // temporanei senza il definitivo, lasciati
  uint16_t failed = 0;                    // cancellazioni non riuscite
  bool     more = false;                  // cartella troppo piena o troppi temporanei: controllata solo in parte
  bool     dryRun = false;                // prova a secco: conta cio che toglierebbe (removed, bytes) ma non cancella niente (vesevos-cleaner)
  uint32_t bytes = 0;                     // byte dei temporanei cancellati (o, in prova a secco, che si cancellerebbero)
  char     firstOrphan[FS_PATH_MAX + 1] = "";   // il primo orfano, per il log
};
void fsTmpClean(const char* dir, FsTmpStat& st);   // una cartella. Lenta con tante voci: da un task di fondo, mai dal web server
void fsTmpCleanKnown(FsTmpStat& st);               // "/", "/scripts", "/apps", "/lang"
