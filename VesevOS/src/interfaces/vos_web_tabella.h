// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_web_tabella.h
// Tabella compatta dei percorsi del server web: ogni voce e {percorso, funzione, metodo, livello} (12 byte).
// Sostituisce un oggetto per percorso (circa 200 byte l'uno, 29 KB per 146 percorsi sulla C3: prova 1.8.23f).
// Senza dipendenze (si prova sul computer). H = tipo del puntatore a funzione (in vos_web_int.h: Handler).
// Si legge dal task del server mentre un altro task aggiunge voci: la voce si scrive per intero PRIMA di alzare il contatore.
#pragma once
#include <stdint.h>
#include <string.h>
#include <atomic>
#include "vos_web_rotte.h"          // RG_GET, RG_POST

template <typename H, int MAX>
class TabellaRotte {
 public:
  struct Voce {
    const char* percorso;           // testo letterale (sta nella flash, non si copia)
    H           funzione;
    uint8_t     metodo;             // RG_GET oppure RG_POST
    uint8_t     livello;            // livello di accesso (Lvl in vos_web_int.h)
  };

  // false = tabella piena (la voce non c'e)
  bool aggiungi(const char* percorso, uint8_t metodo, uint8_t livello, H funzione) {
    const int n = conta_.load(std::memory_order_relaxed);
    if (n >= MAX) return false;
    voci_[n] = Voce{percorso, funzione, metodo, livello};
    conta_.store(n + 1, std::memory_order_release);
    return true;
  }

  // percorso esatto (len = lunghezza senza la parte dopo '?') e metodo uguali; nullptr = non c'e. Vince la prima voce.
  const Voce* cerca(const char* percorso, size_t len, uint8_t metodo) const {
    const int n = conta_.load(std::memory_order_acquire);
    for (int i = 0; i < n; i++) {
      const Voce& v = voci_[i];
      if (v.metodo == metodo && strlen(v.percorso) == len && strncmp(v.percorso, percorso, len) == 0) return &v;
    }
    return nullptr;
  }

  int  conta() const { return conta_.load(std::memory_order_acquire); }
  void svuota() { conta_.store(0, std::memory_order_release); }

 private:
  Voce voci_[MAX];
  std::atomic<int> conta_{0};
};
