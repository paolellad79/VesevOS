// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_cpuboost.cpp (prenotazioni di velocita, vedi vos_cpuboost.h)
#include <mutex>
#include <string.h>
#include "vos_cpuboost.h"
#include "../drivers/vos_drv_time.h"

static std::mutex s_lock;
static uint32_t   s_untilMs = 0;        // scadenza (millisecondi dall'avvio); 0 = nessuna
static char       s_who[12] = "";

// true se ms (ora) e prima della scadenza, anche a cavallo del giro del contatore a 32 bit
static bool before(uint32_t now, uint32_t until) { return (int32_t)(until - now) > 0; }

void cpuBoost(const char* who, uint32_t sec) {
  if (sec == 0) return;
  if (sec > CPUBOOST_MAX_SEC) sec = CPUBOOST_MAX_SEC;
  uint32_t now = (uint32_t)drvMillis();
  uint32_t until = now + sec * 1000UL;
  std::lock_guard<std::mutex> g(s_lock);
  if (s_untilMs == 0 || !before(now, s_untilMs) || before(s_untilMs, until)) s_untilMs = until;   // vale la scadenza piu lontana
  strncpy(s_who, who ? who : "", sizeof(s_who) - 1);
  s_who[sizeof(s_who) - 1] = 0;
}

bool cpuBoostActive() {
  std::lock_guard<std::mutex> g(s_lock);
  return s_untilMs != 0 && before((uint32_t)drvMillis(), s_untilMs);
}

const char* cpuBoostWho() {
  static char copy[sizeof(s_who)];            // copia: chi legge non vede il testo cambiare sotto gli occhi (letto solo dal monitor e dalla shell)
  std::lock_guard<std::mutex> g(s_lock);
  strncpy(copy, s_who, sizeof(copy));
  copy[sizeof(copy) - 1] = 0;
  return copy;
}
