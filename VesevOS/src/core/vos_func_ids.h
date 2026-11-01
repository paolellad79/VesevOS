// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_func_ids.h
// Gli identificatori delle FUNZIONALITA (gruppi di primitive che l'utente puo togliere a tutti i programmi, pannello Sistema > Funzionalita).
// Stanno nel nucleo perche la configurazione li salva come elenco di nomi (sezione [func], chiave "off"); descrizioni, ruoli e collegamento ai
// linguaggi sono in packages/vos_func.h. L'ordine non si cambia: il numero e il bit nella maschera cfg.funcOff.
#pragma once
#include <stddef.h>
#include <string.h>

constexpr const char* kFuncId[] = {"log", "sd", "dev", "gpio", "i2c", "spi", "time", "files", "ble"};
constexpr int kFuncN = (int)(sizeof(kFuncId) / sizeof(kFuncId[0]));

inline int funcIndex(const char* id) {
  if (!id) return -1;
  for (int i = 0; i < kFuncN; i++) if (!strcmp(kFuncId[i], id)) return i;
  return -1;
}
