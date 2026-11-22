// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_rec_file.h
// File CSV del registratore di dati: cartella, spazio massimo, file piu vecchio da togliere, elenco, cancellazione.
#pragma once
#include "../core/vos_errbuf.h"
#include <stddef.h>
#include <stdint.h>

#define REC_DIR_FLASH "/var/lib/rec"
#define REC_DIR_SD    "/media/sd/rec"
#define REC_TIPO_CAMPIONI 0                   // file AAAAMMGG.csv (o rec.csv senza ora)
#define REC_TIPO_EVENTI   1                   // file AAAAMMGG-eventi.csv (o eventi.csv senza ora)
#define REC_FILES_MAX 24                      // voci guardate in una cartella (FS_DIR_MAX e 20 sulla flash)

#define REC_NOME_MAX 24                       // caratteri del nome scelto dall'Admin (lettere, numeri, _ e -)
#define REC_NOME_STD "rec"

struct RecFileInfo {
  char     name[48];                          // <nome>-eventi-AAAAMMGG.csv e il piu lungo: 24 + 8 + 8 + 4
  uint32_t size;
  bool     own;                               // ha il nome attuale: solo questi si contano nel limite e si cancellano per fare posto
  bool     ev;                                // file delle azioni (solo tra i "own")
  uint32_t day;                               // giorno scritto nel nome (-AAAAMMGG), come epoca di mezzogiorno locale; 0 = il nome non ha data
};

bool recNomeValido(const char* nome);         // 1-24 caratteri a-z A-Z 0-9 _ - (mai punti ne barre: il file resta nella cartella del registratore)

const char* recDirOf(int where);              // 0 flash, 1 microSD
// Nomi: campioni <nome>-AAAAMMGG.csv (senza ora valida <nome>.csv), azioni <nome>-eventi-AAAAMMGG.csv (senza ora <nome>-eventi.csv).
// Aggiunge text (n byte) al file di oggi (secondo "tipo", vedi sopra). Se il file e nuovo, o *needHeader e vero, scrive prima la riga
// di intestazione. Prima fa posto: limite 0 = i file piu vecchi si cancellano finche tutti insieme restano sotto maxKb; limite 1 = maxKb vale per
// ogni singolo file (quello di oggi riparte da capo quando lo supera). In ogni caso sotto REC_FILES_MAX - 4 voci.
// Non scrive se lo spazio libero e sotto la soglia di sicurezza (flash 64 KB, microSD 1 MB). *needHeader torna falso a scrittura riuscita.
bool recFileAppend(int where, const char* nome, int maxKb, int limite, int tipo, bool oraValida, const char* header, bool* needHeader, const char* text, size_t n, ErrBuf& err);
int  recFileList(int where, const char* nome, RecFileInfo* out, int max, uint32_t* totalBytes);   // elenco dei file .csv (ordinati per nome); totalBytes = solo i file con il nome attuale; ritorna quanti
bool recFileClear(const char* nome, ErrBuf& err);   // toglie i file .csv con il nome attuale, su flash e su microSD (se montata); quelli con un altro nome non si toccano
