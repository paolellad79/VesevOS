// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_led_nomi.cpp
// Numerazione e nomi vecchi dei LED. Vedi vos_led_nomi.h.
#include "vos_led_nomi.h"
#include "../core/vos_i18n.h"
#include <string.h>

int ledNumeroIniziale(const char* classe) {
  if (!strcmp(classe, "led")) return kLedBordoMono + 1;
  if (!strcmp(classe, "led_rgb")) return kLedBordoRgb + 1;
  return 1;
}

const char* ledNomeVecchioMsg(const char* nome) {
  if (!strcmp(nome, "led")) return kLedBordoRgb ? tr("Nome vecchio: usa led_rgb1") : tr("Nome vecchio: usa led1");
  if (!strcmp(nome, "led2") && kLedBordoMono) return tr("Nome vecchio: usa led1");
  return nullptr;
}
