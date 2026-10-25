// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_fixedstr.h
// Testo breve in un buffer di dimensione fissa N (zero finale compreso): nessuna memoria dinamica, nessun Arduino.
// Serve ai campi della configurazione (nome host, SSID, password...). Se il testo e piu lungo di N - 1 caratteri:
// l'assegnazione con "=" lo taglia (sempre chiuso con 0); set() invece NON cambia nulla e dice false.
#pragma once
#include <stddef.h>
#include <string.h>
#include <string_view>

template <size_t N>
class FixedStr {
  static_assert(N >= 2, "serve spazio per almeno un carattere");
 public:
  FixedStr() { s_[0] = 0; }
  FixedStr(const char* t) { *this = t; }
  FixedStr& operator=(const char* t) {
    size_t n = t ? strlen(t) : 0;
    if (n > N - 1) n = N - 1;
    if (n) memmove(s_, t, n);      // memmove: t puo stare dentro s_ stesso
    s_[n] = 0;
    return *this;
  }
  FixedStr& operator=(std::string_view v) { copy(v.data(), v.size() < N ? v.size() : N - 1); return *this; }
  // Copia solo se ci sta tutto (v puo non avere lo zero finale). false = troppo lungo, il contenuto resta com'era.
  bool set(std::string_view v) {
    if (v.size() > N - 1) return false;
    copy(v.data(), v.size());
    return true;
  }
  void clear() { s_[0] = 0; }

  const char* c_str() const { return s_; }
  std::string_view view() const { return std::string_view(s_, strlen(s_)); }
  size_t length() const { return strlen(s_); }
  bool   empty() const { return s_[0] == 0; }
  char   operator[](size_t i) const { return s_[i]; }
  static constexpr size_t capacity() { return N - 1; }   // caratteri che ci stanno (zero escluso)

  bool operator==(const char* t) const { return strcmp(s_, t ? t : "") == 0; }
  bool operator!=(const char* t) const { return !(*this == t); }

 private:
  void copy(const char* p, size_t n) {
    if (n) memmove(s_, p, n);
    s_[n] = 0;
  }
  char s_[N];
};
