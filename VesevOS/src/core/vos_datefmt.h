// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_datefmt.h
// Formati di data e ora: parte PURA (nessuna configurazione, nessun orologio, nessuna traduzione), cosi si prova sul computer.
// La pagina web ha la stessa logica in web/src/js_dataora.js: le due devono dare lo stesso testo. La prova di parita
// (tools/hosttest/prova_formato_data.sh) legge gli stessi casi da tools/hosttest/casi_formato_data.txt.
//
// Formato scritto a mano (come il comando date della shell), al massimo DF_TESTO_MAX caratteri:
//   %d %-d giorno a due cifre / senza zero      %m %-m mese a due cifre / senza zero
//   %Y anno a 4 cifre   %y anno a 2 cifre        %b mese abbreviato   %B mese completo
//   %a giorno abbreviato  %A giorno completo     %H %-H ore 0-23      %I %-I ore 1-12
//   %M minuti   %S secondi   %p AM/PM            %% il segno di percentuale
// Il resto e testo libero (ASCII, senza virgolette e senza barra rovesciata).
#pragma once
#include <stddef.h>

#define DF_TESTO_MAX 32      // caratteri del formato scritto a mano
#define DF_NOME_MAX  12      // un nome di giorno o di mese, con lo zero finale (il piu lungo ha 10 lettere)
#define DF_FRASE_MAX 24      // "giorno mese anno" di un mese abbreviato, con i due %d (cinese in UTF-8: 18 byte)

enum DfErr { DF_OK = 0, DF_VUOTO, DF_LUNGO, DF_CARATTERE, DF_SEGNO, DF_PERCENTO_FINALE };

struct DfData {
  int anno, mese, giorno;   // mese 1..12
  int giornoSett;           // 0 = domenica
  int ore, minuti, secondi;
};

// Nomi nella lingua scelta. La frase del mese serve alla data abbreviata e al %b: contiene due %d (giorno e anno), e la
// lingua decide ordine e punteggiatura ("%d Nov. %d", "%d. Nov. %d").
struct DfNomi {
  char giornoBreve[7][DF_NOME_MAX];
  char giornoLungo[7][DF_NOME_MAX];
  char meseLungo[12][DF_NOME_MAX];
  char fraseMese[12][DF_FRASE_MAX];
};

// Scelte di Localizzazione.
struct DfImpost {
  int  dataCorta;                 // 0 GG/MM/AAAA, 1 AAAA-MM-GG, 2 MM/GG/AAAA, 3 scritta a mano
  const char* dataCortaTesto;
  int  dataLunga;                 // 0 abbreviata, 1 completa, 2 scritta a mano
  const char* dataLungaTesto;
  bool ampm;                      // ora a 12 ore con AM/PM
  bool oraLungaSec;               // l'ora lunga ha i secondi
  int  oraLunga;                  // 0 pronta, 1 scritta a mano
  const char* oraLungaTesto;
};

DfErr dfControlla(const char* formato);
void  dfFormatta(const char* formato, const DfData& d, const DfNomi* nomi, char* out, size_t n);   // nomi = nullptr: i nomi escono vuoti
// Mese abbreviato ricavato dalla frase del mese: il testo tra i due %d, senza spazi e punti davanti ("Nov.").
void  dfMeseBreve(const char* frase, char* out, size_t n);

void  dfDataCorta(const DfImpost& im, const DfData& d, const DfNomi& nomi, char* out, size_t n);
void  dfDataLunga(const DfImpost& im, const DfData& d, const DfNomi& nomi, char* out, size_t n);
void  dfOraCorta(const DfImpost& im, const DfData& d, char* out, size_t n);    // ore e minuti, mai secondi
void  dfOraLunga(const DfImpost& im, const DfData& d, const DfNomi& nomi, char* out, size_t n);
