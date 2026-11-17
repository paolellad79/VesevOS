// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_region_tab_rule.h
// Regola PURA per la tabella radio dei paesi (file /usr/share/vesevos/radio.txt): una riga per paese "CC canali dBm", righe vuote e commenti con # in prima colonna (di qualunque lunghezza).
// Si danno i byte del file a pezzi qualsiasi (dai) e a fine file fine(); nessun file, nessuna memoria dinamica: provata sul computer
// (build/tests/risorse-file). Una riga non valida rende SCARTATA tutta la tabella (si usa il ripiego prudente: il file e controllato da SHA256SUMS,
// quindi una riga storta e un errore di chi l'ha scritto, non va indovinata).
#pragma once
#include <stddef.h>
#include <stdint.h>

class RegionTabCerca {
 public:
  explicit RegionTabCerca(const char* cc) { cc_[0] = cc ? cc[0] : 0; cc_[1] = cc && cc[0] ? cc[1] : 0; }

  void dai(const char* d, size_t n) {
    for (size_t i = 0; i < n; i++) {
      char c = d[i];
      if (c == '\r') continue;
      if (c == '\n') { riga(); len_ = 0; troppo_ = false; commento_ = false; continue; }
      if (len_ == 0 && c == '#') commento_ = true;          // un commento (# in prima colonna) puo essere lungo quanto vuole: non si legge
      if (commento_) continue;
      if (len_ < sizeof riga_) riga_[len_++] = c;
      else troppo_ = true;
    }
  }

  void fine() { if (len_ || troppo_) riga(); len_ = 0; troppo_ = false; commento_ = false; }

  bool valida() const { return !rotta_; }                              // nessuna riga storta
  bool trovato() const { return !rotta_ && trovato_; }
  int  canali() const { return ch_; }
  int  dbm() const { return dbm_; }
  // Valori piu alti letti in QUALUNQUE riga valida (16/11/2026: solo per un avviso in audit, nessun tetto; il parser accetta fino a 14 canali e 40 dBm)
  int  canaliMax() const { return chMax_; }
  int  dbmMax() const { return dbmMax_; }

 private:
  // riga_ di len_ caratteri: vuota, commento o "CC canali dBm"
  void riga() {
    if (troppo_) { rotta_ = true; return; }
    size_t i = 0;
    while (i < len_ && (riga_[i] == ' ' || riga_[i] == '\t')) i++;
    if (i == len_) return;                                  // riga vuota (o solo spazi)
    if (len_ - i < 6 || !maiuscola(riga_[i]) || !maiuscola(riga_[i + 1]) || riga_[i + 2] != ' ') { rotta_ = true; return; }
    char a = riga_[i], b = riga_[i + 1];
    i += 3;
    int ch = numero(i), dbm = numero(i);
    if (ch < 1 || ch > 14 || dbm < 0 || dbm > 40 || i != len_) { rotta_ = true; return; }
    if (ch > chMax_) chMax_ = ch;
    if (dbm > dbmMax_) dbmMax_ = dbm;
    if (!trovato_ && a == cc_[0] && b == cc_[1]) { trovato_ = true; ch_ = ch; dbm_ = dbm; }
  }

  // un numero senza segno di al massimo 3 cifre da i; poi salta UNO spazio se c'e. -1 se non c'e nessuna cifra o sono troppe.
  int numero(size_t& i) {
    int v = 0, k = 0;
    while (i < len_ && riga_[i] >= '0' && riga_[i] <= '9') { v = v * 10 + (riga_[i] - '0'); i++; k++; if (k > 3) return -1; }
    if (k == 0) return -1;
    if (i < len_ && riga_[i] == ' ') i++;
    return v;
  }

  static bool maiuscola(char c) { return c >= 'A' && c <= 'Z'; }

  char   cc_[2];
  char   riga_[40];
  size_t len_ = 0;
  bool   troppo_ = false;
  bool   commento_ = false;
  bool   rotta_ = false;
  bool   trovato_ = false;
  int    ch_ = 0;
  int    dbm_ = 0;
  int    chMax_ = 0;
  int    dbmMax_ = 0;
};
