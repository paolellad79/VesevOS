// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_vol.h
// Metadati (solo lettura) di cartelle e file, uguali per memoria interna e microSD: elenco a pagine e informazioni su una voce.
// Il percorso e GIA risolto da core/vos_paths (pathResolve): qui "/..." e la memoria interna, "/media" e la cartella virtuale
// (contiene "sd" se la scheda e montata), "/media/sd/..." e la microSD. Non apre il contenuto dei file e non scrive niente.
// Chi chiama passa il ruolo (ROLE_GUEST / ROLE_OPER / ROLE_ADMIN di core/vos_common.h): i file di sistema non si vedono MAI; chiavi,
// certificati e copie .vbk (core/vos_paths: pathIsSecret) li vede solo l'Admin. Una voce nascosta si comporta come se non esistesse.
// Usato da Berry (listdir, stat). Nessun tipo del chip e nessun String.
#pragma once
#include <stdint.h>
#include <stddef.h>

#define VOL_NAME_MAX  63        // caratteri di un nome (zero escluso); le voci con un nome piu lungo non si elencano
#define VOL_PAGE_MAX  32        // voci al massimo per ogni chiamata a volList
#define VOL_START_MAX 2000      // indice di partenza massimo: oltre una cartella cosi grande non si scorre (ogni passo costa tempo)

struct VolEntry {
  char     name[VOL_NAME_MAX + 1];
  bool     dir;
  uint32_t size;                // byte del file; 0 per le cartelle
  uint32_t mtime;               // secondi dal 1970; 0 = data non disponibile (la memoria interna non salva le date)
};

enum VolErr : uint8_t {
  VOL_OK = 0,
  VOL_BAD_ARG,                  // parametri sbagliati (indice fuori limite, nessuna voce richiesta...)
  VOL_BAD_PATH,                 // percorso non ammesso dalla microSD (caratteri strani)
  VOL_NOT_FOUND,                // non c'e, oppure e nascosto per questo ruolo
  VOL_NOT_DIR,                  // e un file, non una cartella
  VOL_NO_MEDIA,                 // la microSD non e montata (o non c'e in questa versione)
  VOL_TOO_LONG,                 // il nome supera VOL_NAME_MAX
  VOL_NO_MEMORY
};

// Elenca una cartella partendo dalla voce "start" (0 = la prima), al massimo "max" voci (1..VOL_PAGE_MAX) in out.
// count = voci scritte; consumed = voci lette dalla cartella (comprese quelle nascoste o con nome troppo lungo): per continuare si richiama
// con start + consumed; more = ce ne sono altre. L'ordine e quello del file system e non cambia tra due chiamate se la cartella non cambia.
VolErr volList(const char* path, int role, int start, VolEntry* out, int max, int& count, int& consumed, bool& more);

// Informazioni su una voce (file o cartella).
VolErr volStat(const char* path, int role, VolEntry& out);
