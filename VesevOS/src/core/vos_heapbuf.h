// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_heapbuf.h
// Testo che cresce sull'heap (malloc/realloc) con un tetto di dimensione. Serve ai JSON la cui lunghezza non si sa prima
// (elenco di una cartella): niente String (frammenta l'heap) e niente std::string (con -fno-exceptions un'allocazione fallita fa abort).
// Se manca la memoria o si supera il tetto, add() ritorna false e non scrive nulla: oom() e over() dicono quale dei due e successo.
// I blocchi sopra 4 KB vanno in PSRAM da soli (CONFIG_SPIRAM_USE_MALLOC con soglia 4096). Non e sicuro tra task: ogni task ha il suo.
#pragma once
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

class HeapBuf {
 public:
  explicit HeapBuf(size_t maxLen) : max_(maxLen) {}
  ~HeapBuf() { free(p_); }
  HeapBuf(const HeapBuf&) = delete;
  HeapBuf& operator=(const HeapBuf&) = delete;

  bool add(const char* s, size_t n) {
    if (oom_) return false;
    if (len_ + n > max_) { over_ = true; return false; }
    if (!grow(n)) return false;
    memcpy(p_ + len_, s, n);
    len_ += n;
    p_[len_] = 0;
    return true;
  }
  bool add(const char* s) { return add(s, strlen(s)); }
  bool add(char c) { return add(&c, 1); }
  // Come printf: il testo formattato si aggiunge in coda. Fino a 160 caratteri usa lo stack, oltre un blocco temporaneo.
  bool addf(const char* fmt, ...) __attribute__((format(printf, 2, 3))) {
    char t[160];
    va_list ap; va_start(ap, fmt);
    int n = vsnprintf(t, sizeof(t), fmt, ap);
    va_end(ap);
    if (n < 0) return false;
    if ((size_t)n < sizeof(t)) return add(t, (size_t)n);
    char* big = (char*)malloc((size_t)n + 1);
    if (!big) { oom_ = true; return false; }
    va_start(ap, fmt);
    vsnprintf(big, (size_t)n + 1, fmt, ap);
    va_end(ap);
    bool ok = add(big, (size_t)n);
    free(big);
    return ok;
  }
  bool addUint(uint32_t v) {
    char t[11];
    int n = snprintf(t, sizeof(t), "%lu", (unsigned long)v);
    return add(t, n > 0 ? (size_t)n : 0);
  }
  // Testo da mettere dentro virgolette JSON, con le stesse regole di jsonEscape(): \" \\ \n \r \t, gli altri caratteri di controllo e quelli
  // sopra 126 diventano '?'.
  bool addJson(const char* s, size_t n) {
    for (size_t i = 0; i < n; i++) {
      unsigned char c = (unsigned char)s[i];
      bool ok;
      if (c == '"') ok = add("\\\"", 2);
      else if (c == '\\') ok = add("\\\\", 2);
      else if (c == '\n') ok = add("\\n", 2);
      else if (c == '\r') ok = add("\\r", 2);
      else if (c == '\t') ok = add("\\t", 2);
      else if (c < 32 || c >= 127) ok = add('?');
      else ok = add((char)c);
      if (!ok) return false;
    }
    return true;
  }

  bool addJson(const char* s) { return addJson(s, strlen(s)); }

  void   cutLast() { if (len_) { len_--; p_[len_] = 0; } }   // toglie l'ultimo carattere (es. la } finale di un JSON a cui aggiungere campi)
  void   clear() { len_ = 0; oom_ = false; over_ = false; if (p_) p_[0] = 0; }   // svuota (la memoria resta per il riuso)
  const char* c_str() const { return p_ ? p_ : ""; }
  size_t length() const { return len_; }
  bool   oom() const { return oom_; }     // manca la memoria
  bool   over() const { return over_; }   // si e superato il tetto

 private:
  bool grow(size_t n) {
    size_t need = len_ + n + 1;             // + lo zero finale
    if (need <= cap_) return true;
    size_t nc = cap_ ? cap_ : 1024;
    while (nc < need) nc *= 2;
    if (nc > max_ + 1) nc = max_ + 1;       // mai oltre il tetto (need e gia <= max_ + 1)
    char* q = (char*)realloc(p_, nc);
    if (!q) { oom_ = true; return false; }  // il vecchio blocco resta valido: lo libera il distruttore
    p_ = q;
    cap_ = nc;
    return true;
  }

  char*  p_ = nullptr;
  size_t cap_ = 0;
  size_t len_ = 0;
  size_t max_;
  bool   oom_ = false;
  bool   over_ = false;
};
