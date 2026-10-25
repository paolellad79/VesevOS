// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_drv_print.h
// STRATO 1 (driver): flusso di uscita testo nostro (OutSink), al posto della classe Print di Arduino.
// Le funzioni che scrivono su "un terminale" (shell, seriale, log, pagina web) prendono un OutSink&.
// Chi lo usa deve solo dire come si scrive un byte (write); print/println/printf sono gia pronti.
#pragma once
#include "vos_drv_string.h"

class OutSink {
public:
  virtual ~OutSink() {}
  virtual size_t write(uint8_t c) = 0;
  virtual size_t write(const uint8_t* b, size_t n) {
    size_t k = 0;
    while (k < n && write(b[k]) == 1) k++;
    return k;
  }
  size_t write(const char* s) { return s ? write((const uint8_t*)s, strlen(s)) : 0; }
  size_t write(const char* s, size_t n) { return s ? write((const uint8_t*)s, n) : 0; }

  size_t print(const char* s) { return write(s); }
  size_t print(char c) { return write((uint8_t)c); }
  size_t print(int v) { return printNum("%d", v); }
  size_t print(unsigned v) { return printNum("%u", v); }
  size_t print(long v) { return printNum("%ld", v); }
  size_t print(unsigned long v) { return printNum("%lu", v); }
  size_t print(long long v) { return printNum("%lld", v); }
  size_t print(unsigned long long v) { return printNum("%llu", v); }
  size_t print(double v) { return printNum("%.2f", v); }
  size_t println() { return write((const uint8_t*)"\r\n", 2); }
  template <typename T> size_t println(T v) { size_t n = print(v); return n + println(); }

  // come printf: prima prova un buffer sullo stack, se il testo e piu lungo usa lo heap
  size_t printf(const char* fmt, ...) __attribute__((format(printf, 2, 3))) {
    char buf[96];
    va_list ap; va_start(ap, fmt);
    va_list ap2; va_copy(ap2, ap);
    int n = vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    size_t out = 0;
    if (n > 0 && (size_t)n < sizeof buf) {
      out = write((const uint8_t*)buf, (size_t)n);
    } else if (n > 0) {
      char* big = (char*)malloc((size_t)n + 1);
      if (big) { vsnprintf(big, (size_t)n + 1, fmt, ap2); out = write((const uint8_t*)big, (size_t)n); free(big); }
    }
    va_end(ap2);
    return out;
  }
  virtual void flush() {}

private:
  template <typename T> size_t printNum(const char* fmt, T v) {
    char buf[32];
    int n = snprintf(buf, sizeof buf, fmt, v);
    return n > 0 ? write((const uint8_t*)buf, (size_t)n) : 0;
  }
};
