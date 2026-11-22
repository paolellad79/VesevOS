// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_risorse_carica_rule.h
// Regole PURE del caricamento di un file singolo di /usr/share/vesevos (non la pagina .vrs: quella ha il suo caricamento in vos_risorse_rule.h).
// Tre casi, scelti dai primi byte del file: (1) CON IMPRONTA INTERNA (dal 21/11/2026): contenitore VOSR formato 2 (SHA-256 dei dati nei byte 64-95)
// oppure testo con la prima riga "# sha256=<64 cifre>" (SHA-256 di tutto il testo DOPO quella riga): il file garantisce se stesso e la scheda
// aggiorna da sola la sua riga in SHA256SUMS; (2) SENZA impronta interna (formato 1, testo vecchio): si accetta solo se il suo SHA-256 intero e quello
// scritto nella riga di SHA256SUMS (come prima); (3) l'elenco SHA256SUMS stesso: ben formato (formato, nomi conosciuti, limiti).
// Il nome e accettato solo se e nell'ELENCO CHIUSO del firmware (RisNomeNoto): un nome sconosciuto e rifiutato. Prima della conferma niente prende
// il posto del file vecchio (temporaneo + scambio nel sink). Provato sul computer (tools/hosttest/prova_risorse_dip.py).
// File system e SHA-256 si passano da fuori (RisFileSink, RisSha).
#pragma once
#include <stdio.h>
#include "vos_risorse_elenco_rule.h"

#define RIS_TESTO_SHA_RIGA 74         // byte della prima riga "# sha256=<64 cifre>\n" dei testi con impronta interna
#define RIS_TESTO_SHA_ETICHETTA "# sha256="

// Dove finisce il file: si apre quando si sa il nome, si scrive, si conferma (scambio in un solo passo) o si butta.
// conferma(aggiornaElenco): se true il sink deve anche portare la riga di questo file in SHA256SUMS (lo SHA-256 del file intero lo calcola lui
// dai byte che ha scritto); se non riesce a preparare l'elenco NON scambia il file e ritorna false (il file vecchio resta).
struct RisFileSink {
  virtual ~RisFileSink() {}
  virtual bool apri(const char* nome, size_t tetto) = 0;
  virtual bool scrivi(const uint8_t* d, size_t n) = 0;
  virtual bool conferma(bool aggiornaElenco) = 0;
  virtual void annulla() = 0;
  virtual bool pieno() const = 0;          // l'ultimo errore e per mancanza di spazio (o tetto della memoria interna)
};

class RisFileScrittore {
 public:
  // sha = SHA-256 del file intero (confronto con la riga di SHA256SUMS), shaInt = SHA-256 della parte garantita dal file stesso: due contesti diversi.
  // fwAttuale: firmware in uso impacchettato (risFwImpacchetta); 0 = non controllare la dipendenza (prove).
  RisFileScrittore(const RisSha& sha, const RisSha& shaInt, RisFileSink& sink, uint32_t fwAttuale = 0)
      : cf_(sha), ci_(shaInt), sink_(sink), fwAttuale_(fwAttuale) {}
  ~RisFileScrittore() { annulla(); }
  RisFileScrittore(const RisFileScrittore&) = delete;
  RisFileScrittore& operator=(const RisFileScrittore&) = delete;

  // Un file singolo. nomeInviato deve essere nell'elenco chiuso (noto): se no RIS_MANCA. elenco/n: testo di SHA256SUMS gia letto dal disco
  // (puo essere vuoto: n = 0); serve solo ai file SENZA impronta interna, e il suo formato si ricontrolla qui. Il nome non e mai un percorso.
  RisEsito iniziaFile(const char* elenco, size_t n, const char* nomeInviato, size_t tetto, RisNomeNoto noto) {
    size_t nl = nomeInviato ? strlen(nomeInviato) : 0;
    if (!risNomeFileOk(nomeInviato, nl) || (noto && !noto(nomeInviato, nl)) || strcmp(nomeInviato, "SHA256SUMS") == 0) return RIS_MANCA;
    memcpy(nome_, nomeInviato, nl);
    nome_[nl] = 0;
    RisRiga r[RIS_ELENCO_RIGHE];
    int k = (elenco && n && risElencoControlla(elenco, n, noto) == RIS_OK) ? risElencoRighe(elenco, n, r) : 0;
    for (int i = 0; i < k && !haAtteso_; i++) {
      if (r[i].nomeLen != nl || memcmp(r[i].nome, nomeInviato, nl) != 0) continue;
      for (int b = 0; b < 32; b++) {
        uint8_t hi = 0, lo = 0;
        risHexVal(r[i].riga[b * 2], hi);
        risHexVal(r[i].riga[b * 2 + 1], lo);
        atteso_[b] = (uint8_t)((hi << 4) | lo);
      }
      haAtteso_ = true;
    }
    tipo_ = T_FILE;
    tetto_ = tetto;
    cf_.avvia(atteso_);                                            // lo SHA-256 intero si calcola sempre: serve se il file non ha impronta interna
    if (!sink_.apri(nome_, tetto_)) { cf_.annulla(); tipo_ = T_NIENTE; return erroreSink(); }
    aperto_ = true;
    return RIS_OK;
  }

  // L'elenco nuovo: si tiene in memoria (RIS_ELENCO_MAX byte), si controlla alla fine e solo allora si scrive.
  RisEsito iniziaElenco(RisNomeNoto noto) {
    noto_ = noto;
    tipo_ = T_ELENCO;
    tetto_ = RIS_ELENCO_MAX;
    return RIS_OK;
  }

  RisEsito dai(const uint8_t* d, size_t n) {
    if (tipo_ == T_NIENTE || fine_) return RIS_SCRITTURA;
    if (ricevuti_ + n > tetto_) { annulla(); return RIS_GRANDE; }
    if (tipo_ == T_ELENCO) {
      memcpy(buf_ + ricevuti_, d, n);
    } else {
      if (ricevuti_ < RIS_TESTO_SCAN) {                              // i primi byte servono a leggere versione, dipendenza e impronta (stesso buffer dell'elenco)
        size_t k = RIS_TESTO_SCAN - ricevuti_;
        memcpy(buf_ + ricevuti_, d, n < k ? n : k);
      }
      cf_.dai(d, n);
      if (n && !sink_.scrivi(d, n)) { RisEsito e = erroreSink(); annulla(); return e; }
    }
    size_t prima = ricevuti_;
    ricevuti_ += n;
    if (tipo_ == T_FILE) {
      sceglieModo();
      daiInterno(d, n, prima);
    }
    if (tipo_ == T_FILE && !dipControllata_ && ricevuti_ >= RIS_TESTO_SCAN) {   // dipendenza non soddisfatta: si smette subito, niente resta scritto
      RisEsito e = controllaDipendenza();
      if (e != RIS_OK) { annulla(); return e; }
    }
    return RIS_OK;
  }

  RisEsito fine() {
    if (tipo_ == T_NIENTE || fine_) return RIS_SCRITTURA;
    fine_ = true;
    if (ricevuti_ == 0) { annullaSink(); return RIS_TRONCATA; }
    if (tipo_ == T_ELENCO) return fineElenco();
    if (!dipControllata_) {
      RisEsito e = controllaDipendenza();
      if (e != RIS_OK) { annulla(); return e; }
    }
    if (modo_ == M_INDEF) modo_ = M_VECCHIO;                         // meno di 9 byte: nessuna impronta interna
    RisEsito e = modo_ == M_VECCHIO ? fineVecchio() : modo_ == M_BIN ? fineBinario() : fineTesto();
    if (e != RIS_OK) { annullaSink(); return e; }
    bool ok = sink_.conferma(modo_ != M_VECCHIO);                    // con impronta interna la scheda aggiorna da sola la sua riga di SHA256SUMS
    aperto_ = false;
    return ok ? RIS_OK : erroreSink();
  }

  void annulla() {
    if (tipo_ == T_FILE) { cf_.annulla(); ci_.annulla(); }
    annullaSink();
    fine_ = true;
  }

  const char* nome() const { return nome_; }
  size_t      ricevuti() const { return ricevuti_; }
  bool        improntaInterna() const { return modo_ == M_BIN || modo_ == M_TESTO; }

 private:
  enum Tipo : uint8_t { T_NIENTE, T_FILE, T_ELENCO };
  enum Modo : uint8_t { M_INDEF, M_VECCHIO, M_BIN, M_TESTO };

  // Dai primi 9 byte: VOSR formato 2 o piu = impronta nell'intestazione (dati dal byte 96); "# sha256=" = impronta nella prima riga (testo dal byte 74).
  // Tutto il resto (anche VOSR formato 1) e "vecchio": serve la riga di SHA256SUMS.
  void sceglieModo() {
    if (modo_ != M_INDEF || ricevuti_ < 9) return;
    if (memcmp(buf_, "VOSR", 4) == 0) {
      modo_ = (uint8_t)buf_[4] >= 2 ? M_BIN : M_VECCHIO;
    } else if (memcmp(buf_, RIS_TESTO_SHA_ETICHETTA, 9) == 0) {
      modo_ = M_TESTO;
    } else {
      modo_ = M_VECCHIO;
    }
    if (modo_ == M_BIN || modo_ == M_TESTO) {
      inizio_ = modo_ == M_BIN ? RIS_INTEST : RIS_TESTO_SHA_RIGA;
      uint8_t zero[32] = {0};
      ci_.avvia(zero);
    }
  }

  // Passa a ci_ la parte del pezzo (d, n, che comincia al byte "prima" del file) che sta dopo l'inizio dei dati garantiti.
  void daiInterno(const uint8_t* d, size_t n, size_t prima) {
    if (modo_ != M_BIN && modo_ != M_TESTO) return;
    size_t fineP = prima + n;
    if (fineP <= inizio_) return;
    size_t da = prima > inizio_ ? prima : inizio_;
    ci_.dai(d + (da - prima), fineP - da);
  }

  // Versione e dipendenza dentro il file (contenitore VOSR o righe "# dipende=" in cima al testo): firmware troppo vecchio = RIS_DIPENDE.
  RisEsito controllaDipendenza() {
    dipControllata_ = true;
    if (fwAttuale_ == 0) return RIS_OK;
    size_t len = ricevuti_ < RIS_TESTO_SCAN ? ricevuti_ : RIS_TESTO_SCAN;
    if (len >= 4 && memcmp(buf_, "VOSR", 4) == 0) {
      RisInfo i;
      if (risIntestazione((const uint8_t*)buf_, len, 0xFFFFFFFFu, i) != RIS_OK) return RIS_FORMATO;
      return i.fwMin > fwAttuale_ ? RIS_DIPENDE : RIS_OK;
    }
    RisTesto t = risTestoLeggi(buf_, len);
    if (!t.ok) return RIS_FORMATO;
    return t.fwMin > fwAttuale_ ? RIS_DIPENDE : RIS_OK;
  }

  // Senza impronta interna: serve la riga di SHA256SUMS e lo SHA-256 del file intero deve essere uguale.
  RisEsito fineVecchio() {
    if (!haAtteso_) { cf_.annulla(); return RIS_MANCA; }
    return cf_.fine() ? RIS_OK : RIS_SHA;
  }

  // Contenitore VOSR formato 2: intestazione valida, nome dentro = nome del file (senza .vgz), lunghezza esatta, SHA-256 dei dati = byte 64-95.
  RisEsito fineBinario() {
    cf_.annulla();
    uint8_t h[32];
    ci_.chiudi(h);
    RisInfo i;
    if (ricevuti_ < RIS_INTEST) return RIS_TRONCATA;
    RisEsito e = risIntestazione((const uint8_t*)buf_, RIS_INTEST, (uint32_t)tetto_, i);
    if (e != RIS_OK) return e;
    size_t nl = strlen(nome_);
    if (nl < 5 || strcmp(nome_ + nl - 4, ".vgz") != 0 || strlen(i.nome) != nl - 4 || memcmp(i.nome, nome_, nl - 4) != 0) return RIS_FORMATO;
    if (ricevuti_ < (size_t)RIS_INTEST + i.lunghezza) return RIS_TRONCATA;
    if (ricevuti_ > (size_t)RIS_INTEST + i.lunghezza) return RIS_ECCESSO;
    return uguali(h, i.sha) ? RIS_OK : RIS_SHA;
  }

  // Testo: "# sha256=" + 64 cifre minuscole + a-capo, poi il testo; lo SHA-256 e quello di tutto il testo dopo quella riga.
  RisEsito fineTesto() {
    cf_.annulla();
    uint8_t h[32];
    ci_.chiudi(h);
    size_t nl = strlen(nome_);
    if (nl < 5 || strcmp(nome_ + nl - 4, ".txt") != 0) return RIS_FORMATO;
    if (ricevuti_ <= RIS_TESTO_SHA_RIGA || buf_[RIS_TESTO_SHA_RIGA - 1] != '\n') return RIS_FORMATO;
    uint8_t scritto[32];
    for (int b = 0; b < 32; b++) {
      uint8_t hi = 0, lo = 0;
      if (!risHexVal(buf_[9 + b * 2], hi) || !risHexVal(buf_[10 + b * 2], lo)) return RIS_FORMATO;
      scritto[b] = (uint8_t)((hi << 4) | lo);
    }
    return uguali(h, scritto) ? RIS_OK : RIS_SHA;
  }

  static bool uguali(const uint8_t* a, const uint8_t* b) {
    uint8_t d = 0;
    for (int i = 0; i < 32; i++) d |= (uint8_t)(a[i] ^ b[i]);       // senza uscita anticipata
    return d == 0;
  }

  RisEsito erroreSink() const { return sink_.pieno() ? RIS_SPAZIO : RIS_SCRITTURA; }

  RisEsito fineElenco() {
    RisEsito e = risElencoControlla(buf_, ricevuti_, noto_);
    if (e != RIS_OK) return e;
    snprintf(nome_, sizeof nome_, "SHA256SUMS");
    if (!sink_.apri(nome_, RIS_ELENCO_MAX)) return erroreSink();
    aperto_ = true;
    if (!sink_.scrivi((const uint8_t*)buf_, ricevuti_)) { RisEsito e2 = erroreSink(); annullaSink(); return e2; }
    bool ok = sink_.conferma(false);
    aperto_ = false;
    return ok ? RIS_OK : erroreSink();
  }
  void annullaSink() {
    if (aperto_) sink_.annulla();
    aperto_ = false;
  }

  RisConfronto  cf_;
  RisConfronto  ci_;
  RisFileSink&  sink_;
  Tipo          tipo_ = T_NIENTE;
  Modo          modo_ = M_INDEF;
  RisNomeNoto   noto_ = nullptr;
  char          nome_[RIS_FILE_NOME_MAX + 1] = "";
  uint8_t       atteso_[32] = {0};
  bool          haAtteso_ = false;
  char          buf_[RIS_ELENCO_MAX];
  size_t        tetto_ = 0;
  size_t        inizio_ = 0;
  size_t        ricevuti_ = 0;
  bool          aperto_ = false;
  bool          fine_ = false;
  uint32_t      fwAttuale_;
  bool          dipControllata_ = false;
};
