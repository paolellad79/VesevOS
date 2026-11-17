// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_shell_man_rule.h
// Regola PURA per leggere il file del manuale (/usr/share/vesevos/manuale.txt): niente file, niente shell, provata sul computer
// (build/tests/risorse-file). Formato: tools/manuale_lib.py. Il testo arriva a pezzi da una funzione di lettura; per la voce cercata si
// chiamano le funzioni date: una per ogni riga di descrizione, una per ogni esempio, una per il rimando delle voci lunghe.
#pragma once
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define MAN_RIGA_MAX 320                  // byte di una riga (le righe piu lunghe si tagliano)

enum ManEsito { MAN_FILE_NO = -1, MAN_NO = 0, MAN_BREVE = 1, MAN_LUNGA = 2 };

// leggi(uint8_t* buf, size_t n) -> byte letti (0 a fine file). desc(riga), es(riga), lungo() sono richiamati nell'ordine del file.
template <class Leggi, class Desc, class Es, class Lungo>
ManEsito manScorri(const char* nome, Leggi leggi, Desc desc, Es es, Lungo lungo) {
  uint8_t buf[128];
  char riga[MAN_RIGA_MAX];
  size_t n = 0, pos = 0, fine = 0;
  ManEsito st = MAN_NO;
  bool dentro = false, ultima = false;
  while (!ultima) {
    bool completa = false;
    while (!completa) {                                   // una riga alla volta
      if (pos == fine) {
        fine = leggi(buf, sizeof buf);
        pos = 0;
        if (fine == 0) {
          ultima = true;
          break;
        }
      }
      char c = (char)buf[pos++];
      if (c == '\n') completa = true;
      else if (c != '\r' && n + 1 < sizeof riga) riga[n++] = c;
    }
    if (!completa && n == 0) break;                       // fine del file senza altro testo
    riga[n] = 0;
    n = 0;
    if (riga[0] == '#' || riga[0] == 0) continue;
    if (riga[0] == '@') {
      if (dentro) break;                                  // la voce cercata e finita
      dentro = strcmp(riga + 1, nome) == 0;
      if (dentro) st = MAN_BREVE;
    } else if (!dentro) {
      continue;
    } else if (riga[0] == '>') {
      st = MAN_LUNGA;
      lungo();
    } else if (riga[0] == '$' && riga[1] == ' ') {
      es(riga + 2);
    } else {
      desc(riga);
    }
  }
  return st;
}
