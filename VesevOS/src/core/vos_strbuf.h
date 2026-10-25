// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_strbuf.h
// Scrittore di testo su un buffer fisso che sceglie il chiamante: nessuna memoria dinamica, nessun Arduino.
// Il testo e sempre chiuso con 0 (se la capacita e almeno 1). Se non ci sta tutto, si tronca e si ricorda:
// truncated() dice se e successo, required() quanti caratteri servivano (zero finale escluso).
#pragma once
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <string>

class StrBuf {
 public:
  StrBuf(char* buf, size_t cap) : b_(buf), cap_(cap) { if (cap_) b_[0] = 0; }

  StrBuf& add(const char* s) { return addn(s, s ? strlen(s) : 0); }
  StrBuf& add(char c) { return addn(&c, 1); }
  StrBuf& addn(const char* s, size_t n) {
    need_ += n;
    if (cap_ == 0) return *this;
    size_t room = cap_ - 1 - len_;
    size_t k = n < room ? n : room;
    if (k) { memcpy(b_ + len_, s, k); len_ += k; }
    b_[len_] = 0;
    return *this;
  }
  StrBuf& addu(uint32_t v) {
    char t[11]; int n = snprintf(t, sizeof(t), "%lu", (unsigned long)v);
    return addn(t, n > 0 ? (size_t)n : 0);
  }
  StrBuf& addf(const char* fmt, ...) __attribute__((format(printf, 2, 3))) {
    va_list ap; va_start(ap, fmt);
    char* dst = cap_ ? b_ + len_ : nullptr;
    size_t room = cap_ ? cap_ - len_ : 0;
    int n = vsnprintf(dst, room, fmt, ap);
    va_end(ap);
    if (n > 0) { need_ += (size_t)n; size_t k = (size_t)n < room ? (size_t)n : (room ? room - 1 : 0); len_ += k; }
    return *this;
  }

  const char* c_str() const { return cap_ ? b_ : ""; }
  size_t length() const { return len_; }
  size_t required() const { return need_; }
  bool   truncated() const { return need_ > len_; }

 private:
  char*  b_;
  size_t cap_;
  size_t len_ = 0;    // caratteri scritti
  size_t need_ = 0;   // caratteri che servirebbero per scrivere tutto
};

// Testo di dimensione massima nota (da 1 a ~4 KB) costruito con StrBuf su UN solo blocco di heap, poi consegnato come std::string
// della lunghezza giusta. Per i testi piccoli e per quelli senza limite certo non serve: li fa StrBuf su un buffer del chiamante.
class StrText {
 public:
  explicit StrText(size_t cap) : s_(cap ? cap : 1, '\0'), sb_(&s_[0], s_.size()) {}
  StrBuf& buf() { return sb_; }
  bool truncated() const { return sb_.truncated(); }
  std::string take() { s_.resize(sb_.length()); return std::move(s_); }   // dopo take() non si scrive piu
 private:
  std::string s_;
  StrBuf sb_;
};
