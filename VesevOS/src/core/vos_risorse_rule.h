// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_risorse_rule.h
// Regole PURE delle risorse (file .vrs): formato, controlli e scrittura a pezzi. Nessun file, nessun hardware, nessun IDF:
// provato sul computer con file veri (build/tests/risorse). SHA-256 e file system si passano da fuori (RisSha, RisSink),
// il firmware li collega in vos_risorse.cpp. Il formato e descritto in tools/mkrisorse.py (intestazione di 96 byte + dati).
// Formato 1 = file vecchi (byte 6-7 e 60-63 a zero). Formato 2 (dal 1.8.30) = versione propria della risorsa (byte 6-7) e firmware minimo
// (byte 60-63). Lo stesso contenitore serve per .vrs e .vgz (gzip). Il firmware 1.8.29d accetta solo il formato 1 e rifiuta il 2 senza scrivere niente.
#pragma once
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define RIS_INTEST       96            // byte dell'intestazione
#define RIS_FORMATO_VER  2             // versione piu alta del formato che questo firmware sa leggere (accetta anche la 1)
#define RIS_ID_LEN       16            // cifre dell'identita del file (informazione: il firmware non la confronta con niente)
#define RIS_NOME_CAMPO   32            // byte del campo nome (31 caratteri + zero)
#define RIS_TETTO_BYTE   (256u * 1024u)   // dati al massimo (la pagina oggi: 143.427)

enum RisEsito : uint8_t {
  RIS_OK = 0,
  RIS_MANCA,        // il file non c'e
  RIS_FORMATO,      // marchio, versione, byte riservati, codifica o nome non validi
  RIS_GRANDE,       // lunghezza zero o oltre il tetto
  RIS_TRONCATA,     // meno byte di quelli dichiarati
  RIS_ECCESSO,      // piu byte di quelli dichiarati
  RIS_SHA,          // SHA-256 dei dati diverso da quello scritto
  RIS_LETTURA,      // errore leggendo il file
  RIS_SCRITTURA,    // errore scrivendo (apertura, scambio finale)
  RIS_SPAZIO,       // manca lo spazio (solo caricamento dei file singoli: vos_risorse_carica_rule.h)
  RIS_DIPENDE       // la risorsa chiede un firmware piu nuovo di quello in uso
};

struct RisInfo {
  char     nome[RIS_NOME_CAMPO];
  char     id[RIS_ID_LEN + 1];
  uint32_t lunghezza;      // byte di dati
  uint8_t  codifica;       // 0 = grezzi, 1 = gzip
  uint8_t  verMag;         // versione della risorsa: parte maggiore e minore (formato 1: 0.0 = non dichiarata)
  uint8_t  verMin;
  uint32_t fwMin;          // firmware minimo impacchettato (risFwImpacchetta); 0 = nessuna dipendenza
  uint8_t  sha[32];        // SHA-256 dei dati, come scritto nell'intestazione
};

// Nome valido: da 1 a 31 caratteri a-z 0-9 - _ (diventa /ui/<nome>.vrs: niente punti ne barre).
inline bool risNomeOk(const char* s, size_t n) {
  if (n < 1 || n >= RIS_NOME_CAMPO) return false;
  for (size_t i = 0; i < n; i++) {
    char c = s[i];
    bool ok = (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_';
    if (!ok) return false;
  }
  return true;
}

// Controlla i primi RIS_INTEST byte. tetto: dati al massimo. L'identita (byte 8-23) deve essere scritta bene ma non si confronta con un valore
// del firmware (16/11/2026: il firmware non ha ancora un id delle risorse; l'integrita sta nello SHA-256 dei dati e nelle righe di SHA256SUMS).
inline RisEsito risIntestazione(const uint8_t* h, size_t n, uint32_t tetto, RisInfo& out) {
  if (n < RIS_INTEST) return RIS_TRONCATA;
  if (memcmp(h, "VOSR", 4) != 0 || h[4] < 1 || h[4] > RIS_FORMATO_VER || h[5] > 1) return RIS_FORMATO;
  bool v1 = h[4] == 1;                                          // formato 1: versione e dipendenza non esistono, i byte restano a zero
  if (v1 && (h[6] != 0 || h[7] != 0 || h[60] != 0 || h[61] != 0 || h[62] != 0 || h[63] != 0)) return RIS_FORMATO;
  for (int i = 0; i < RIS_ID_LEN; i++) {
    char c = (char)h[8 + i];
    if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return RIS_FORMATO;
  }
  size_t nl = 0;
  while (nl < RIS_NOME_CAMPO && h[24 + nl] != 0) nl++;
  for (size_t i = nl; i < RIS_NOME_CAMPO; i++) if (h[24 + i] != 0) return RIS_FORMATO;   // dopo il nome solo zeri
  if (!risNomeOk((const char*)h + 24, nl)) return RIS_FORMATO;
  uint32_t len = (uint32_t)h[56] | ((uint32_t)h[57] << 8) | ((uint32_t)h[58] << 16) | ((uint32_t)h[59] << 24);
  if (len == 0 || len > tetto) return RIS_GRANDE;
  memset(&out, 0, sizeof out);
  memcpy(out.nome, h + 24, nl);
  memcpy(out.id, h + 8, RIS_ID_LEN);
  out.lunghezza = len;
  out.codifica = h[5];
  out.verMag = h[6];
  out.verMin = h[7];
  out.fwMin = ((uint32_t)h[60] << 24) | ((uint32_t)h[61] << 16) | ((uint32_t)h[62] << 8) | (uint32_t)h[63];
  memcpy(out.sha, h + 64, 32);
  return RIS_OK;
}

// ---- Versione del firmware impacchettata in 32 bit: maggiore, minore, patch, lettera (0 = nessuna, 1 = a...). Si confronta come numero. ----
// "1.8.30" -> 0x01081E00 ; "1.8.29d" -> 0x01081D04. 0 se il testo non e fatto cosi.
inline uint32_t risFwImpacchetta(const char* t) {
  if (!t) return 0;
  uint32_t parte[3] = {0, 0, 0};
  for (int i = 0; i < 3; i++) {
    if (*t < '0' || *t > '9') return 0;
    uint32_t v = 0;
    while (*t >= '0' && *t <= '9') {
      v = v * 10 + (uint32_t)(*t - '0');
      if (v > 255) return 0;
      t++;
    }
    parte[i] = v;
    if (i < 2) {
      if (*t != '.') return 0;
      t++;
    }
  }
  uint32_t lettera = 0;
  if (*t >= 'a' && *t <= 'z') lettera = (uint32_t)(*t - 'a' + 1), t++;
  if (*t != 0) return 0;
  return (parte[0] << 24) | (parte[1] << 16) | (parte[2] << 8) | lettera;
}

// Versione della risorsa in testo ("5" se la parte minore e zero, "1.2" altrimenti; "" se non dichiarata). out almeno 8 byte.
inline void risVersioneTesto(uint8_t mag, uint8_t mi, char* out, size_t cap) {
  if (cap == 0) return;
  out[0] = 0;
  if (mag == 0 && mi == 0) return;
  if (mi == 0) snprintf(out, cap, "%u", (unsigned)mag);
  else snprintf(out, cap, "%u.%u", (unsigned)mag, (unsigned)mi);
}

// File di testo: cerca le righe "# versione=N" (o N.M) e "# dipende=firmware>=X.Y.Z" nei primi byte. Tutto il resto si ignora.
// Trovate: versione != 0 e/o fwMin != 0. Una riga con un valore non valido rende il file non valido (ok = false).
#define RIS_TESTO_SCAN 1024            // byte iniziali in cui si cercano le righe
struct RisTesto {
  uint8_t  verMag = 0;
  uint8_t  verMin = 0;
  uint32_t fwMin = 0;
  bool     ok = true;
};
inline RisTesto risTestoLeggi(const char* t, size_t n) {
  RisTesto r;
  if (n > RIS_TESTO_SCAN) n = RIS_TESTO_SCAN;
  size_t pos = 0;
  while (pos < n && t[pos] == '#') {
    const char* nl = (const char*)memchr(t + pos, '\n', n - pos);
    size_t len = nl ? (size_t)(nl - (t + pos)) : n - pos;
    char riga[48];
    if (len >= 2 && len < sizeof riga) {
      memcpy(riga, t + pos, len);
      riga[len] = 0;
      if (strncmp(riga, "# versione=", 11) == 0) {
        unsigned a = 0, b = 0;
        int k = sscanf(riga + 11, "%u.%u", &a, &b);
        if (k < 1 || a > 255 || b > 255 || a == 0) r.ok = false;
        else { r.verMag = (uint8_t)a; r.verMin = k == 2 ? (uint8_t)b : 0; }
      } else if (strncmp(riga, "# dipende=firmware>=", 20) == 0) {
        uint32_t f = risFwImpacchetta(riga + 20);
        if (f == 0) r.ok = false;
        else r.fwMin = f;
      }
    }
    if (!nl) break;
    pos += len + 1;
  }
  return r;
}

// SHA-256 a pezzi, fornito da chi usa questa classe (nel firmware: mbedTLS, con l'acceleratore hardware dell'IDF).
struct RisSha {
  void* ctx;
  void (*avvia)(void* ctx);
  void (*aggiungi)(void* ctx, const uint8_t* d, size_t n);
  void (*chiudi)(void* ctx, uint8_t out[32]);
};

// Controllo a pezzi di un file .vrs intero (intestazione + dati): si danno i byte nell'ordine in cui arrivano, poi fine().
// Il primo errore resta (sticky). Dopo l'intestazione buona si sa gia lunghezza e nome; lo SHA-256 si controlla solo in fine().
class RisVerifica {
 public:
  // fwAttuale: firmware in uso impacchettato (risFwImpacchetta); 0 = non controllare la dipendenza (prove).
  explicit RisVerifica(const RisSha& sha, uint32_t tetto = RIS_TETTO_BYTE, uint32_t fwAttuale = 0) : sha_(sha), tetto_(tetto), fwAttuale_(fwAttuale) {}
  ~RisVerifica() { chiudiSha(); }                               // se ci si ferma a meta il contesto SHA va liberato lo stesso
  RisVerifica(const RisVerifica&) = delete;
  RisVerifica& operator=(const RisVerifica&) = delete;

  RisEsito dai(const uint8_t* d, size_t n) {
    if (err_ != RIS_OK) return err_;
    if (nh_ < RIS_INTEST) {
      size_t k = RIS_INTEST - nh_;
      if (k > n) k = n;
      memcpy(hdr_ + nh_, d, k);
      nh_ += k;
      d += k;
      n -= k;
      if (nh_ == RIS_INTEST) {
        err_ = risIntestazione(hdr_, RIS_INTEST, tetto_, info_);
        if (err_ != RIS_OK) return err_;
        if (fwAttuale_ != 0 && info_.fwMin > fwAttuale_) { err_ = RIS_DIPENDE; return err_; }
        sha_.avvia(sha_.ctx);
        shaAperto_ = true;
      }
    }
    if (n) {
      if (nh_ < RIS_INTEST) return RIS_OK;                       // intestazione ancora incompleta (n e 0 qui, per sicurezza)
      if (n > info_.lunghezza - corpo_) { err_ = RIS_ECCESSO; return err_; }
      sha_.aggiungi(sha_.ctx, d, n);
      corpo_ += (uint32_t)n;
    }
    return RIS_OK;
  }

  // A fine file: tutti i byte arrivati e SHA-256 uguale a quello scritto.
  RisEsito fine() {
    if (err_ != RIS_OK) return err_;
    if (nh_ < RIS_INTEST || corpo_ < info_.lunghezza) { err_ = RIS_TRONCATA; return err_; }
    uint8_t h[32];
    chiudiSha(h);
    uint8_t d = 0;
    for (int i = 0; i < 32; i++) d |= (uint8_t)(h[i] ^ info_.sha[i]);   // senza uscita anticipata: stesso tempo comunque
    if (d != 0) err_ = RIS_SHA;
    return err_;
  }

  bool           intestOk() const { return nh_ == RIS_INTEST && err_ == RIS_OK; }   // intestazione completa e buona
  const uint8_t* intest() const { return hdr_; }
  const RisInfo& info() const { return info_; }
  RisEsito       errore() const { return err_; }
  size_t         ricevuti() const { return nh_ + corpo_; }

 private:
  void chiudiSha(uint8_t* out = nullptr) {
    if (!shaAperto_) return;
    uint8_t scarto[32];
    sha_.chiudi(sha_.ctx, out ? out : scarto);
    shaAperto_ = false;
  }

  RisSha   sha_;
  uint32_t tetto_;
  uint32_t fwAttuale_;
  uint8_t  hdr_[RIS_INTEST];
  size_t   nh_ = 0;
  uint32_t corpo_ = 0;
  RisInfo  info_ = {};
  RisEsito err_ = RIS_OK;
  bool     shaAperto_ = false;
};

// Dove finisce il file: si apre solo a intestazione buona (il nome sta nell'intestazione), si scrive, si conferma o si butta.
// conferma() deve sostituire il file vero in un solo passo (temporaneo + rinomina): un file a meta non compare mai col nome vero.
struct RisSink {
  virtual ~RisSink() {}
  virtual bool apri(const RisInfo& info) = 0;
  virtual bool scrivi(const uint8_t* d, size_t n) = 0;
  virtual bool conferma() = 0;
  virtual void annulla() = 0;
};

// Scrittura a pezzi di una risorsa che arriva (caricamento, microSD, rete): controlla tutto prima di confermare.
// Prima di apri() non si scrive niente; al primo errore si annulla da soli. Un file alterato o troncato non diventa mai il file vero.
class RisScrittore {
 public:
  RisScrittore(const RisSha& sha, RisSink& sink, uint32_t tetto = RIS_TETTO_BYTE, uint32_t fwAttuale = 0) : ver_(sha, tetto, fwAttuale), sink_(sink) {}
  ~RisScrittore() { annulla(); }
  RisScrittore(const RisScrittore&) = delete;
  RisScrittore& operator=(const RisScrittore&) = delete;

  RisEsito dai(const uint8_t* d, size_t n) {
    if (fatto_) return RIS_SCRITTURA;
    size_t prima = ver_.ricevuti();
    RisEsito e = ver_.dai(d, n);
    if (e != RIS_OK) { annulla(); return e; }
    if (!aperto_) {
      if (!ver_.intestOk()) return RIS_OK;                       // l'intestazione non e ancora tutta arrivata
      if (!sink_.apri(ver_.info())) { fatto_ = true; return RIS_SCRITTURA; }   // apri() fallita non lascia niente da buttare
      aperto_ = true;                                            // da qui un errore deve passare da annulla() (butta il temporaneo)
      if (!sink_.scrivi(ver_.intest(), RIS_INTEST)) { annulla(); return RIS_SCRITTURA; }
      size_t inInt = RIS_INTEST - prima;                         // quanti byte di questo pezzo erano intestazione (gia scritta)
      d += inInt;
      n -= inInt;
    }
    if (n && !sink_.scrivi(d, n)) { annulla(); return RIS_SCRITTURA; }
    return RIS_OK;
  }

  // Ultimo passo: controlli finali e, solo se tutto torna, scambio col file vero.
  RisEsito fine() {
    if (fatto_) return RIS_SCRITTURA;
    RisEsito e = ver_.fine();
    if (e == RIS_OK && !aperto_) e = RIS_TRONCATA;
    if (e == RIS_OK && !sink_.conferma()) e = RIS_SCRITTURA;
    else if (e != RIS_OK && aperto_) sink_.annulla();
    aperto_ = false;
    fatto_ = true;
    return e;
  }

  void annulla() {
    if (aperto_) sink_.annulla();
    aperto_ = false;
    fatto_ = true;
  }

  const RisInfo& info() const { return ver_.info(); }

 private:
  RisVerifica ver_;
  RisSink&    sink_;
  bool        aperto_ = false;
  bool        fatto_ = false;
};

// Nome tecnico dell'esito (solo ASCII, per i registri e le prove; il testo per l'utente lo sceglie chi chiama).
inline const char* risCodice(RisEsito e) {
  switch (e) {
    case RIS_OK:        return "ok";
    case RIS_MANCA:     return "manca";
    case RIS_FORMATO:   return "formato";
    case RIS_GRANDE:    return "grande";
    case RIS_TRONCATA:  return "troncata";
    case RIS_ECCESSO:   return "eccesso";
    case RIS_SHA:       return "sha";
    case RIS_LETTURA:   return "lettura";
    case RIS_SCRITTURA: return "scrittura";
    case RIS_SPAZIO:    return "spazio";
    case RIS_DIPENDE:   return "dipende";
  }
  return "?";
}
