// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_shell_line.h
// Riga di comando della shell senza String: parole copiate in un buffer fisso (Word), resto della riga come puntatore,
// riga in digitazione sulla seriale in un buffer fisso. Nessun Arduino: si prova anche sul computer.
#pragma once
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

// Una parola della riga. Sta in un buffer fisso (niente heap). Se la parola e piu lunga di MAX tiene i primi MAX caratteri e
// cut() diventa true: MAX e sopra tutti i limiti dei comandi (percorso 100, rete 32, password 63), quindi una parola
// tagliata viene rifiutata dal controllo del comando e non diventa mai un'altra parola valida.
class Word {
 public:
  static const size_t MAX = 103;
  Word() { b_[0] = 0; }
  Word(const char* s, size_t n) {
    if (n > MAX) { n = MAX; cut_ = true; }
    memcpy(b_, s, n);
    b_[n] = 0;
    len_ = n;
  }
  const char* c_str() const { return b_; }
  size_t length() const { return len_; }
  bool   empty() const { return len_ == 0; }
  bool   cut() const { return cut_; }
  bool   operator==(const char* s) const { return strcmp(b_, s) == 0; }
  bool   operator!=(const char* s) const { return strcmp(b_, s) != 0; }
  bool   startsWith(const char* p) const { return strncmp(b_, p, strlen(p)) == 0; }
  char   last() const { return len_ ? b_[len_ - 1] : 0; }
  long   toInt() const { return strtol(b_, nullptr, 10); }
  Word&  lower() { for (size_t i = 0; i < len_; i++) if (b_[i] >= 'A' && b_[i] <= 'Z') b_[i] = (char)(b_[i] + 32); return *this; }
  Word&  upper() { for (size_t i = 0; i < len_; i++) if (b_[i] >= 'a' && b_[i] <= 'z') b_[i] = (char)(b_[i] - 32); return *this; }

 private:
  char   b_[MAX + 1];
  size_t len_ = 0;
  bool   cut_ = false;
};

namespace shx {
Word        argAt(const char* line, int idx);        // parola numero idx della riga (0 = comando); vuota se non c'e
const char* restFrom(const char* line, int idx);     // il resto della riga dalla parola idx (punta dentro la riga; "" se non c'e)
// Un tasto sulla riga in digitazione (buffer buf di cap byte, lunghezza len): Backspace cancella, Ctrl+U/Ctrl+C svuotano, i tasti
// freccia (ESC...) si scartano. erase = quanti "\b \b" mostrare al terminale, typed = carattere da mostrare (0 = nessuno).
// Ritorna false se il tasto non e gestito (lo decide chi chiama).
bool        editKey(char* buf, size_t& len, size_t cap, uint8_t& esc, char ch, int& erase, char& typed);
}  // namespace shx
