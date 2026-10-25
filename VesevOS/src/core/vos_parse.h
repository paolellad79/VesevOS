// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_parse.h
// Aiuti per leggere testi senza String e senza memoria dinamica: lavorano su std::string_view (una vista, non copiano).
// Fanno lo stesso di String::trim() e String::toInt() di Arduino, cosi si puo sostituire una String senza cambiare il comportamento.
// Attenzione: la vista non possiede il testo, che deve restare valido finche la si usa.
#pragma once
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <string_view>

// Come String::trim() di Arduino-ESP32 3.3: toglie da entrambi i lati TUTTI gli spazi bianchi (isspace: spazio, tab, \r, \n, \v, \f).
inline std::string_view trimView(std::string_view v) {
  while (!v.empty() && isspace((unsigned char)v.front())) v.remove_prefix(1);
  while (!v.empty() && isspace((unsigned char)v.back())) v.remove_suffix(1);
  return v;
}

// Come String::toInt() (che chiama atol): salta gli spazi iniziali, legge segno e cifre, si ferma al primo carattere non valido, 0 se non c'e un numero.
// La vista non ha lo zero finale: si copia in un piccolo buffer (oltre 31 caratteri si legge solo l'inizio; un numero cosi lungo satura comunque).
inline long viewToLong(std::string_view v) {
  char t[32];
  size_t n = v.size() < sizeof(t) - 1 ? v.size() : sizeof(t) - 1;
  memcpy(t, v.data(), n);
  t[n] = 0;
  return atol(t);
}
