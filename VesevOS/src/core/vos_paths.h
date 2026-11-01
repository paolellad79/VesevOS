// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_paths.h
// UNICA risoluzione dei percorsi, per memoria interna e microSD: usata dalla shell, da Berry e (tramite fsCleanTo) dall'API dei file.
//   - memoria interna: alla radice, con le cartelle di vos_layout.h ("/sbin/a.vsb"; "/scripts/a.vsb" resta valido come scorciatoia)
//   - microSD: sotto "/media/sd". "/media" e una cartella virtuale (non esiste sulla memoria interna): contiene "sd" se la scheda e montata.
//   - "/sd" e "/sd/x" restano validi in input come scorciatoia: diventano "/media/sd" e "/media/sd/x". Solo il nome intero: "/sdx" e
//     "/media/sdcard" NON sono la microSD. Dopo la traduzione il percorso non puo superare FS_PATH_MAX caratteri.
// La normalizzazione di un argomento (pathResolve) sta in vos_pathres.h. Questo file non decide chi puo fare cosa e non guarda se la scheda e montata: sono cose degli strati sopra.
#pragma once
#include <stddef.h>

// Traduce sul posto la scorciatoia "/sd" in "/media/sd" (path: percorso gia pulito; cap: spazio disponibile).
// true se non c'era niente da tradurre o se e stato tradotto; false se il risultato supera maxLen caratteri (zero escluso) o non ci sta in cap.
bool pathSdShortcut(char* path, size_t cap, size_t maxLen);

// Scorciatoia dei nomi vecchi (passo B, vedi vos_layout.h): a copia finita "/apps/x" vale "/bin/x", "/scripts" "/sbin", "/backup" "/var/backup"
// e "/audit.log" "/crash.txt" "/boots3.bin" stanno in /var. Prima della copia (o se non e riuscita) vale il contrario: i nomi nuovi
// diventano quelli vecchi, che sono quelli in uso. Stessi argomenti di pathSdShortcut; false se il risultato e troppo lungo.
bool pathLayoutShortcut(char* path, size_t cap, size_t maxLen);

// Il percorso pulito e "/media" o sta sotto "/media"?
bool pathIsMedia(const char* clean);
// Il percorso pulito e proprio "/media"?
bool pathIsMediaRoot(const char* clean);
// Se il percorso pulito e "/media/sd" o sta sotto: la parte sulla scheda ("/" oppure "/cartella/file"), altrimenti nullptr.
// Il puntatore e dentro clean: non va liberato.
const char* pathSdRel(const char* clean);
// La parte sulla scheda ("/" oppure "/cartella/file", cioe cio che dice pathSdRel) ha un nome ammesso dalla microSD? Solo ASCII sicuro (lettere, numeri,
// spazio e _ - . ( ) /), mai "..", "//", nomi che cominciano con spazio o finiscono con spazio o punto (FatFs li taglia). Al massimo 100 caratteri.
bool pathSdRelOk(const char* rel);

// File di sistema nascosti a tutti (configurazione, registro di audit, rapporto del crash). Controlla anche la parte sulla microSD.
bool pathIsSystemFile(const char* clean);
// Come sopra, piu i file che portano chiavi, certificati o copie di configurazione (.pem .key .crt .der .p12 .vbk, senza badare alle
// maiuscole). E' la STESSA regola per archivi, elenchi e Berry, su memoria interna e microSD.
bool pathIsSecret(const char* clean);
