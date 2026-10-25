// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_wd_rule.h
// Regola dell'antiloop del watchdog, senza dipendenze (cosi si prova sul computer).
#pragma once
#include <stdint.h>

// Finestra di plausibilita: un orario ricordato nel futuro di oltre WD_FUTURE_MAX_S non e credibile (dato corrotto)
// e viene ignorato. 900 = salvataggio del contaore ogni 600 s + margine (ipotesi, non misurata).
static const int32_t WD_FUTURE_MAX_S = 900;

// secondi di quanto c (secondi di vita) supera il contaore attuale; 0 se non e nel futuro. Con segno: niente underflow.
static inline uint32_t wdFutureSec(uint32_t now, uint32_t c) {
  int32_t age = (int32_t)(now - c);
  return age < 0 ? (uint32_t)(-(int64_t)age) : 0;
}

// true se c e nel futuro oltre la finestra: dato da ignorare (e da segnalare nel log)
static inline bool wdFutureBad(uint32_t now, uint32_t c) {
  return c && wdFutureSec(now, c) > (uint32_t)WD_FUTURE_MAX_S;
}

// true se l'ultimo dei 3 riavvii automatici ricordati (c, in secondi di vita) e ancora dentro l'ora.
// c == 0: non ricordato. La differenza e CON SEGNO: se c e poco nel futuro (contaore tornato indietro, fino a
// WD_FUTURE_MAX_S) conta come recente: in dubbio meglio fermare i riavvii. Oltre la finestra e ignorato.
static inline bool wdTooMany(uint32_t now, uint32_t c) {
  if (!c || wdFutureBad(now, c)) return false;
  int32_t age = (int32_t)(now - c);
  return age < 3600;
}
