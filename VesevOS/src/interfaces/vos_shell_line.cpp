// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_shell_line.cpp
#include "vos_shell_line.h"

Word shx::argAt(const char* line, int idx) {
  int n = 0;
  const char* s = line;
  while (*s) {
    while (*s == ' ') s++;
    const char* b = s;
    while (*s && *s != ' ') s++;
    if (s > b) { if (n == idx) return Word(b, (size_t)(s - b)); n++; }
  }
  return Word();
}

const char* shx::restFrom(const char* line, int idx) {
  int n = 0;
  const char* s = line;
  while (*s) {
    while (*s == ' ') s++;
    if (!*s) break;
    if (n == idx) return s;
    while (*s && *s != ' ') s++;
    n++;
  }
  return "";
}

bool shx::editKey(char* buf, size_t& len, size_t cap, uint8_t& esc, char ch, int& erase, char& typed) {
  erase = 0; typed = 0;
  if (esc == 1) { esc = (ch == '[') ? 2 : (ch == 'O') ? 3 : 0; return true; }
  if (esc == 2) { if ((uint8_t)ch >= 0x40 && (uint8_t)ch <= 0x7E) esc = 0; return true; }
  if (esc == 3) { esc = 0; return true; }
  if (ch == 27) { esc = 1; return true; }
  if (ch == 8 || ch == 127) { if (len) { buf[--len] = 0; erase = 1; } return true; }
  if (ch == 21 || ch == 3) { erase = (int)len; len = 0; buf[0] = 0; return true; }
  if (ch >= 32 && ch < 127) { if (len < cap) { buf[len++] = ch; buf[len] = 0; typed = ch; } return true; }
  return false;
}
