// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_i18n_calc.h
// Parte PURA delle traduzioni (nessuna dipendenza da FreeRTOS, file system o configurazione): formato del dizionario
// `.vld`, ricerca a salti sul file e memoria circolare dei testi recenti. Serve al firmware (vos_i18n.cpp) e alla prova
// sul computer (build/tests/lingue). Come funziona vos_region_calc.h per la radio.
//
// FORMATO .vld (numeri a 32 bit little endian; lo scrive tools/mklang.py):
//   byte  0-3   marchio "VOSL"          byte 4  versione del formato (1)      byte 5-7  zero
//   byte  8-11  n = numero di voci      byte 12-15  posizione della tabella   byte 16-19  posizione dei testi
//   byte 20-23  dimensione totale       byte 24-31  zero
//   byte 32...  testata di testo: righe "nome=...", "locale=...", "bandiera=<svg...>" chiuse da \n (fino alla tabella)
//   tabella     n voci da 10 byte ORDINATE per impronta: impronta u32, posizione u32 (dall'inizio dei testi), lunghezza u16
//   testi       UTF-8 uno dopo l'altro, senza terminatore
// Impronta = FNV-1a a 32 bit dei byte UTF-8 della frase italiana. Una frase mancante = si usa la frase italiana.
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>

#define LANG_VOCE        10      // byte di una voce della tabella
#define LANG_INTEST      32      // byte dell'intestazione
#define LANG_BLOCCO      16      // sotto queste voci la ricerca legge tutto il pezzo in una lettura (160 byte di stack)
#define LANG_TESTO_MAX   640     // testo piu lungo ammesso (il piu lungo oggi: 631 byte; mklang controlla)
#define LANG_ARENA       4096    // memoria circolare dei testi recenti
#define LANG_SLOT        48      // voci ricordate (testi e assenze)
#define LANG_RINNOVO     512     // un testo ricordato da piu di questi byte di inserimenti si rilegge (vedi LangCache)

static inline uint32_t langImpronta(const char* s, size_t n) {
  uint32_t h = 0x811c9dc5u;
  for (size_t i = 0; i < n; i++) {
    h ^= (uint8_t)s[i];
    h *= 0x01000193u;
  }
  return h;
}

static inline uint32_t langU32(const uint8_t* p) {
  return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static inline uint16_t langU16(const uint8_t* p) {
  return (uint16_t)(p[0] | (p[1] << 8));
}

// Intestazione letta dal file (16 byte di RAM).
struct LangVld {
  uint32_t n = 0;
  uint32_t offTab = 0;
  uint32_t offTesti = 0;
  uint32_t totale = 0;
};

// Controlla i 32 byte iniziali e la dimensione vera del file. false = non e un dizionario valido.
static inline bool langVldIntest(const uint8_t* h, size_t dimFile, LangVld& d) {
  if (memcmp(h, "VOSL", 4) != 0 || h[4] != 1) return false;
  d.n = langU32(h + 8);
  d.offTab = langU32(h + 12);
  d.offTesti = langU32(h + 16);
  d.totale = langU32(h + 20);
  if (d.n == 0 || d.n > 65535) return false;
  if (d.offTab < LANG_INTEST) return false;
  if (d.offTesti != d.offTab + d.n * LANG_VOCE) return false;
  return d.totale == dimFile && d.offTesti <= d.totale;
}

// Il testo di una voce sta dentro il file? Se no, 'fuori' (se c'e) diventa true: il file non e quello caricato.
static inline bool voceDentro(const LangVld& d, uint32_t pos, uint16_t lun, bool* fuori) {
  bool dentro = (uint64_t)pos + lun <= (uint64_t)d.totale - d.offTesti;
  if (!dentro && fuori) *fuori = true;
  return dentro;
}

// Ricerca binaria dell'impronta h nella tabella, a salti sul file. 'leggi' e un oggetto con
//   bool operator()(uint32_t posizione, void* buf, size_t n)   (legge n byte a partire da posizione; true se li ha letti tutti)
// e un campo bool 'errore' che il lettore mette a true se una lettura fallisce (solo langTraduci lo guarda).
// Nessun caricamento intero: 10 byte per passo, gli ultimi LANG_BLOCCO in una lettura sola.
// true = trovata: pos e lun sono la posizione (dall'inizio dei testi) e la lunghezza del testo, verificate dentro il file.
template <class Lettore>
static bool langVldCerca(Lettore& leggi, const LangVld& d, uint32_t h, uint32_t& pos, uint16_t& lun, bool* fuori = nullptr) {
  int64_t lo = 0;
  int64_t hi = (int64_t)d.n - 1;
  uint8_t v[LANG_BLOCCO * LANG_VOCE];
  while (lo <= hi) {
    size_t resto = (size_t)(hi - lo + 1);
    if (resto <= LANG_BLOCCO) {
      if (!leggi(d.offTab + (uint32_t)lo * LANG_VOCE, v, resto * LANG_VOCE)) return false;
      for (size_t i = 0; i < resto; i++) {
        const uint8_t* e = v + i * LANG_VOCE;
        if (langU32(e) != h) continue;
        pos = langU32(e + 4);
        lun = langU16(e + 8);
        return voceDentro(d, pos, lun, fuori);                              // una voce fuori dal file non vale
      }
      return false;
    }
    int64_t mid = (lo + hi) / 2;
    if (!leggi(d.offTab + (uint32_t)mid * LANG_VOCE, v, LANG_VOCE)) return false;
    uint32_t x = langU32(v);
    if (x == h) {
      pos = langU32(v + 4);
      lun = langU16(v + 8);
      return voceDentro(d, pos, lun, fuori);
    }
    if (x < h) lo = mid + 1;
    else hi = mid - 1;
  }
  return false;
}

// Valore di una riga "chiave=valore" della testata (chiave con il segno =, es. "nome="). Copia in out (tagliato se non ci sta).
// false = riga assente o valore tagliato.
static inline bool langTestataValore(const char* raw, size_t len, const char* chiave, char* out, size_t cap) {
  out[0] = 0;
  size_t kl = strlen(chiave);
  size_t i = 0;
  while (i < len) {
    size_t fine = i;
    while (fine < len && raw[fine] != '\n') fine++;
    if (fine - i >= kl && memcmp(raw + i, chiave, kl) == 0) {
      size_t n = fine - i - kl;
      bool intero = n + 1 <= cap;
      if (!intero) n = cap - 1;
      memcpy(out, raw + i + kl, n);
      out[n] = 0;
      return intero;
    }
    i = fine + 1;
  }
  return false;
}

// Memoria circolare dei testi tradotti di recente (ARENA byte + SLOT voci) e delle assenze (frasi uguali all'italiano:
// si ricorda che nel file non ci sono, senza rileggerlo). Non ha blocchi: chi la usa la protegge con un mutex.
//
// GARANZIA SUI PUNTATORI. tr() restituisce un puntatore dentro l'arena. Resta valido, cioe il testo non cambia, finche
// dopo la chiamata si inseriscono (si leggono dal file) meno di LANG_GARANZIA byte di testi NUOVI (testo + zero finale).
// Con testi medi di 50-60 byte sono circa 30 frasi diverse; con testi di 600 byte solo 2-3. I testi gia ricordati non
// contano: non si inseriscono di nuovo. Calcolo: l'arena e a giro; un testo di s byte puo essere cancellato dopo che
// sono entrati almeno ARENA - s - 2*(MAX+1) byte (il resto dell'arena in fondo che non basta per il testo seguente, al
// piu MAX+1, e il pezzo all'inizio che il testo seguente copre, al piu MAX+1); con s <= MAX+1 vale ARENA - 3*(MAX+1).
// Un testo ricordato da piu di LANG_RINNOVO byte di inserimenti non si restituisce piu com'e: si rilegge e si
// reinserisce, cosi il puntatore che esce da OGNI chiamata ha davanti almeno LANG_GARANZIA byte.
// Chi tiene il puntatore piu a lungo lo copia (tr() e trf() restano validi senza copia per l'uso nella stessa istruzione).
#define LANG_GARANZIA (LANG_ARENA - 3 * (LANG_TESTO_MAX + 1) - LANG_RINNOVO)

class LangCache {
 public:
  LangCache() { svuota(); }

  // Dimentica tutte le voci (cambio lingua o file). L'arena non si tocca: i puntatori gia dati restano validi come prima.
  void svuota() {
    for (int i = 0; i < LANG_SLOT; i++) slot_[i].stato = VUOTO;
    attesa_ = -1;
  }

  // Cerca h tra i ricordi. true = si sa la risposta: testo = puntatore al testo, oppure nullptr se nel file non c'e (assenza).
  bool trova(uint32_t h, const char*& testo) {
    for (int i = 0; i < LANG_SLOT; i++) {
      Slot& s = slot_[i];
      if (s.stato == VUOTO || s.h != h) continue;
      if (s.stato == ASSENTE) {
        testo = nullptr;
        return true;
      }
      if (inseriti_ - s.quando > LANG_RINNOVO) {      // troppo vecchio: si rilegge (vedi la garanzia)
        s.stato = VUOTO;
        return false;
      }
      testo = arena_ + s.off;
      return true;
    }
    return false;
  }

  void ricordaAssente(uint32_t h) {
    Slot& s = prossimo();
    s.h = h;
    s.stato = ASSENTE;
  }

  // Prepara lo spazio per un testo di lun byte (piu lo zero). Ritorna dove scriverlo, nullptr se non entra (lun > MAX).
  // Va seguita da conferma() se la lettura riesce; altrimenti non si fa nulla.
  char* riserva(uint32_t h, uint16_t lun) {
    if (lun == 0 || lun > LANG_TESTO_MAX) return nullptr;
    int dim = (int)lun + 1;
    if (testa_ + dim > LANG_ARENA) testa_ = 0;        // a capo
    cancella(testa_, testa_ + dim);
    attesa_ = (int)(&prossimo() - slot_);
    slot_[attesa_].stato = VUOTO;
    slot_[attesa_].h = h;
    slot_[attesa_].off = (uint16_t)testa_;
    slot_[attesa_].lun = lun;
    char* dove = arena_ + testa_;
    testa_ += dim;
    inseriti_ += (uint32_t)dim;
    return dove;
  }

  // La lettura e riuscita: scrive lo zero finale e rende valida la voce. Ritorna il puntatore al testo.
  const char* conferma() {
    Slot& s = slot_[attesa_];
    arena_[s.off + s.lun] = 0;
    s.stato = TESTO;
    s.quando = inseriti_;
    attesa_ = -1;
    return arena_ + s.off;
  }

  uint32_t inseriti() const { return inseriti_; }      // byte inseriti in totale (per le prove)
  const char* arena() const { return arena_; }

 private:
  enum : uint8_t { VUOTO = 0, TESTO = 1, ASSENTE = 2 };
  struct Slot {
    uint32_t h;
    uint32_t quando;       // valore di inseriti_ dopo l'inserimento
    uint16_t off;
    uint16_t lun;
    uint8_t stato;
  };

  Slot& prossimo() {
    Slot& s = slot_[prossimo_];
    prossimo_ = (prossimo_ + 1) % LANG_SLOT;
    return s;
  }

  // Le voci il cui testo cade (anche in parte) in [da, a) non valgono piu.
  void cancella(int da, int a) {
    for (int i = 0; i < LANG_SLOT; i++) {
      Slot& s = slot_[i];
      if (s.stato == TESTO && s.off < a && (int)s.off + s.lun + 1 > da) s.stato = VUOTO;
    }
  }

  char arena_[LANG_ARENA];
  Slot slot_[LANG_SLOT];
  int testa_ = 0;           // prossimo punto libero dell'arena
  int prossimo_ = 0;        // prossima voce da riusare (a giro)
  int attesa_ = -1;         // voce riservata e non ancora confermata
  uint32_t inseriti_ = 0;   // byte inseriti da sempre (testo + zero)
};

// Ricerca completa di una frase: ricordi, poi file. 'leggi' come in langVldCerca. Ritorna il testo tradotto (puntatore
// nell'arena) o nullptr se non c'e (si usa la frase italiana). NON protegge dagli altri task: il chiamante tiene il mutex.
template <class Lettore>
static const char* langTraduci(LangCache& c, Lettore& leggi, const LangVld& d, const char* it, size_t n, bool* fuori = nullptr) {
  uint32_t h = langImpronta(it, n);
  const char* t;
  if (c.trova(h, t)) return t;
  uint32_t pos = 0;
  uint16_t lun = 0;
  if (!langVldCerca(leggi, d, h, pos, lun, fuori) || lun == 0) {
    if (!leggi.errore && !(fuori && *fuori)) c.ricordaAssente(h);            // un guasto di lettura non e un'assenza: si riprova alla prossima
    return nullptr;
  }
  char* dove = c.riserva(h, lun);
  if (!dove) return nullptr;                           // troppo lungo per la memoria: si usa l'italiano
  if (!leggi(d.offTesti + pos, dove, lun)) return nullptr;
  return c.conferma();
}
