// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_i18n_salute_rule.h
// Parte PURA del controllo di salute del file di lingua in uso (nessuna dipendenza da FreeRTOS o file system; la prova sul
// computer e in build/tests/lingue/prova_salute.cpp). tr() riapre il .vld a ogni ricerca vera: a ogni apertura controlla che il file sia
// ancora quello caricato. Un file sostituito di nascosto (fs/ren, fs/copy, fs/save, unzip, cp...) non deve far leggere
// posizioni del vecchio file dentro il nuovo.
//
// Un guasto "subito" mette la lingua in errore: da quel momento tr() usa l'italiano senza aprire piu nulla.
//   LG_NON_SI_APRE  il file non c'e piu (o al suo posto c'e una cartella)
//   LG_DIMENSIONE   la dimensione non e quella registrata al caricamento
//   LG_INTESTAZIONE l'intestazione non e piu valida o non coincide con quella registrata
//   LG_POSIZIONE    una voce della tabella punta fuori dal file
// Un errore di lettura (il file si apre, ma seek o read falliscono) conta: LANG_LETTURE_MAX ricerche vere di fila con un errore
// di lettura mettono la lingua in errore (LG_LETTURE). Una ricerca riuscita azzera il conto.
// Perche 3: un errore isolato (un blocco flash difettoso toccato da una sola frase, una lettura contesa) non deve togliere la
// lingua; tre ricerche vere consecutive che falliscono, con frasi diverse, indicano un guasto che dura. Il valore e piccolo
// perche ogni ricerca fallita costa I/O inutile, e una lingua in errore e comunque facile da ripristinare.
#pragma once
#include <stdint.h>
#include <stddef.h>
#include "vos_i18n_calc.h"

enum LangGuasto : uint8_t {
  LG_NESSUNO = 0,
  LG_NON_SI_APRE = 1,
  LG_DIMENSIONE = 2,
  LG_INTESTAZIONE = 3,
  LG_POSIZIONE = 4,
  LG_LETTURE = 5,
};

#define LANG_LETTURE_MAX 3

// Conto degli errori di lettura consecutivi (una byte di RAM).
struct LangLetture {
  uint8_t n = 0;
  bool fallita() {                       // true = raggiunto il limite
    if (n < 255) n++;
    return n >= LANG_LETTURE_MAX;
  }
  void riuscita() { n = 0; }
};

// Il file aperto e ancora quello caricato? 'h' = i primi LANG_INTEST byte, 'dim' = dimensione vera del file.
static inline LangGuasto langControllaApertura(uint32_t dim, const uint8_t* h, const LangVld& atteso) {
  if (dim != atteso.totale) return LG_DIMENSIONE;
  LangVld d;
  if (!langVldIntest(h, dim, d)) return LG_INTESTAZIONE;
  if (d.n != atteso.n || d.offTab != atteso.offTab || d.offTesti != atteso.offTesti || d.totale != atteso.totale) return LG_INTESTAZIONE;
  return LG_NESSUNO;
}

// Lettore con controllo di salute per langTraduci. 'Fonte' offre:
//   bool apri();                  apre il file (false = non si apre; una cartella non conta come file)
//   bool esiste();                dopo un apri() fallito: il file c'e ancora (un errore di apertura che puo passare)?
//   uint32_t dimensione();        dimensione del file aperto
//   bool leggi(uint32_t pos, void* buf, size_t n);   legge n byte da pos, true se li ha letti tutti
// Si apre alla PRIMA lettura (se la frase e tra i ricordi non si apre nulla). Campi di esito per tr():
//   errore  una lettura e fallita (la frase non va ricordata come assente)
//   guasto  il file non e piu quello caricato (vedi sopra); errore e vero anche in questo caso
//   fuori   una voce della tabella punta fuori dal file (impostato da langTraduci)
//   aperto  il file e stato aperto e controllato
template <class Fonte>
class LangLettore {
 public:
  LangLettore(Fonte& f, const LangVld& atteso) : f_(f), atteso_(atteso) {}

  bool operator()(uint32_t pos, void* buf, size_t n) {
    if (guasto != LG_NESSUNO) return false;
    if (!provato_) {
      provato_ = true;
      aperto = apriEControlla();
    }
    if (!aperto) {
      errore = true;
      return false;
    }
    if (!f_.leggi(pos, buf, n)) {
      errore = true;
      return false;
    }
    return true;
  }

  bool errore = false;
  LangGuasto guasto = LG_NESSUNO;
  bool fuori = false;
  bool aperto = false;

 private:
  bool apriEControlla() {
    if (!f_.apri()) {
      if (!f_.esiste()) guasto = LG_NON_SI_APRE;
      return false;                                  // se il file c'e ma non si apre e un errore di lettura (si conta)
    }
    uint32_t dim = f_.dimensione();
    uint8_t h[LANG_INTEST];
    if (dim < LANG_INTEST) {
      guasto = LG_DIMENSIONE;
      return false;
    }
    if (!f_.leggi(0, h, sizeof h)) return false;     // errore di lettura (si conta)
    guasto = langControllaApertura(dim, h, atteso_);
    return guasto == LG_NESSUNO;
  }

  Fonte& f_;
  const LangVld& atteso_;
  bool provato_ = false;
};

// Esito di una ricerca vera: il guasto da registrare (LG_NESSUNO = la lingua resta in uso).
// 'letture' si aggiorna: errore di lettura = conta, ricerca riuscita = azzera.
static inline LangGuasto langEsitoRicerca(bool aperto, LangGuasto guasto, bool errore, bool fuori, LangLetture& letture) {
  if (guasto != LG_NESSUNO) return guasto;
  if (fuori) return LG_POSIZIONE;
  if (errore) return letture.fallita() ? LG_LETTURE : LG_NESSUNO;
  if (aperto) letture.riuscita();
  return LG_NESSUNO;
}
