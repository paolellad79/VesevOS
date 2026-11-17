// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_risorse_carica_rule.h
// Regole PURE del caricamento di un file singolo di /usr/share/vesevos (non la pagina .vrs: quella ha il suo caricamento in vos_risorse_rule.h).
// Due casi: (1) un file dell'elenco SHA256SUMS: si accetta solo se il suo SHA-256 e quello scritto nella riga dell'elenco, con un tetto di
// dimensione; (2) l'elenco SHA256SUMS stesso: si accetta se e ben formato (formato, nomi conosciuti, limiti). Il nome del file NON si prende da chi invia: lo
// da la riga dell'elenco. Prima della conferma niente prende il posto del file vecchio (temporaneo + scambio nel sink). Provato sul computer
// (build/tests/risorse-file). File system e SHA-256 si passano da fuori (RisFileSink, RisSha).
#pragma once
#include <stdio.h>
#include "vos_risorse_elenco_rule.h"

// Dove finisce il file: si apre quando si sa il nome (dall'elenco), si scrive, si conferma (scambio in un solo passo) o si butta.
struct RisFileSink {
  virtual ~RisFileSink() {}
  virtual bool apri(const char* nome, size_t tetto) = 0;
  virtual bool scrivi(const uint8_t* d, size_t n) = 0;
  virtual bool conferma() = 0;
  virtual void annulla() = 0;
  virtual bool pieno() const = 0;          // l'ultimo errore e per mancanza di spazio (o tetto della memoria interna)
};

class RisFileScrittore {
 public:
  RisFileScrittore(const RisSha& sha, RisFileSink& sink) : cf_(sha), sink_(sink) {}
  ~RisFileScrittore() { annulla(); }
  RisFileScrittore(const RisFileScrittore&) = delete;
  RisFileScrittore& operator=(const RisFileScrittore&) = delete;

  // Un file dell'elenco. elenco/n: testo di SHA256SUMS gia letto dal disco (si ricontrolla qui il formato). nomeInviato: il nome che manda chi
  // carica (solo per cercarlo nell'elenco, mai usato come percorso). RIS_FORMATO = elenco non buono, RIS_MANCA = il nome non e nell'elenco.
  RisEsito iniziaFile(const char* elenco, size_t n, const char* nomeInviato, size_t tetto, RisNomeNoto noto) {
    RisEsito e = risElencoControlla(elenco, n, noto);
    if (e != RIS_OK) return e;
    RisRiga r[RIS_ELENCO_RIGHE];
    int k = risElencoRighe(elenco, n, r);
    size_t nl = nomeInviato ? strlen(nomeInviato) : 0;
    for (int i = 0; i < k; i++) {
      if (r[i].nomeLen != nl || memcmp(r[i].nome, nomeInviato, nl) != 0) continue;
      memcpy(nome_, r[i].nome, r[i].nomeLen);                       // il nome vero e quello dell'elenco
      nome_[r[i].nomeLen] = 0;
      for (int b = 0; b < 32; b++) {
        uint8_t hi = 0, lo = 0;
        risHexVal(r[i].riga[b * 2], hi);
        risHexVal(r[i].riga[b * 2 + 1], lo);
        atteso_[b] = (uint8_t)((hi << 4) | lo);
      }
      tipo_ = T_FILE;
      tetto_ = tetto;
      cf_.avvia(atteso_);
      if (!sink_.apri(nome_, tetto_)) { cf_.annulla(); tipo_ = T_NIENTE; return erroreSink(); }
      aperto_ = true;
      return RIS_OK;
    }
    return RIS_MANCA;
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
      cf_.dai(d, n);
      if (n && !sink_.scrivi(d, n)) { RisEsito e = erroreSink(); annulla(); return e; }
    }
    ricevuti_ += n;
    return RIS_OK;
  }

  RisEsito fine() {
    if (tipo_ == T_NIENTE || fine_) return RIS_SCRITTURA;
    fine_ = true;
    if (ricevuti_ == 0) { annullaSink(); return RIS_TRONCATA; }
    if (tipo_ == T_ELENCO) return fineElenco();
    bool uguale = cf_.fine();
    if (!uguale) { annullaSink(); return RIS_SHA; }
    bool ok = sink_.conferma();
    aperto_ = false;
    return ok ? RIS_OK : erroreSink();
  }

  void annulla() {
    if (tipo_ == T_FILE) cf_.annulla();
    annullaSink();
    fine_ = true;
  }

  const char* nome() const { return nome_; }
  size_t      ricevuti() const { return ricevuti_; }

 private:
  enum Tipo : uint8_t { T_NIENTE, T_FILE, T_ELENCO };

  RisEsito erroreSink() const { return sink_.pieno() ? RIS_SPAZIO : RIS_SCRITTURA; }

  RisEsito fineElenco() {
    RisEsito e = risElencoControlla(buf_, ricevuti_, noto_);
    if (e != RIS_OK) return e;
    snprintf(nome_, sizeof nome_, "SHA256SUMS");
    if (!sink_.apri(nome_, RIS_ELENCO_MAX)) return erroreSink();
    aperto_ = true;
    if (!sink_.scrivi((const uint8_t*)buf_, ricevuti_)) { RisEsito e2 = erroreSink(); annullaSink(); return e2; }
    bool ok = sink_.conferma();
    aperto_ = false;
    return ok ? RIS_OK : erroreSink();
  }
  void annullaSink() {
    if (aperto_) sink_.annulla();
    aperto_ = false;
  }

  RisConfronto  cf_;
  RisFileSink&  sink_;
  Tipo          tipo_ = T_NIENTE;
  RisNomeNoto   noto_ = nullptr;
  char          nome_[RIS_FILE_NOME_MAX + 1] = "";
  uint8_t       atteso_[32] = {0};
  char          buf_[RIS_ELENCO_MAX];
  size_t        tetto_ = 0;
  size_t        ricevuti_ = 0;
  bool          aperto_ = false;
  bool          fine_ = false;
};
