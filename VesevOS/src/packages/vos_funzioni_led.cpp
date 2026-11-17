// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_funzioni_led.cpp: funzioni VesevOS dei LED di bordo (spostate da vos_funzioni.cpp, file sotto le 500 righe).
//   led_rgb1("RRGGBB" | "auto")       LED a colori: prende il colore SUBITO (qualunque modo sia scelto), non salva nulla; "auto" torna il modo normale.
//                                     A fine script il LED torna normale da solo.
//   led1("on" | "off" | "blink")      LED monocromatico di bordo, se la scheda ce l'ha. Non salva nulla; a fine script resta com'e.
//   led(...) e led2(...)              nomi VECCHI (decisione dell'utente del 13/11/2026): non valgono piu, danno un errore che dice il nome nuovo.
//                                     Gli script e le app gia scritti con questi nomi vanno corretti a mano.
#include "vos_funzioni_int.h"
#include "../core/vos_common.h"
#if VOS_WITH_FUNZIONI
#include "../third_party/funzioni/src/funzioni.h"
#include "../core/vos_i18n.h"
#include "../devices/vos_led.h"
#include "../devices/vos_led2.h"
#include "../devices/vos_led_nomi.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

static int bfLedRgb(bvm* vm) {
  if (be_top(vm) < 1 || !be_isstring(vm, 1)) be_raise(vm, "type_error", "led_rgb1(\"RRGGBB\"): serve un testo di 6 cifre esadecimali (o \"auto\")");
  const char* c = be_tostring(vm, 1);
  if (strcmp(c, "auto") == 0) { ledScript(-1); be_return_nil(vm); }
  bool okc = strlen(c) == 6;
  for (int i = 0; okc && i < 6; i++) okc = isxdigit((unsigned char)c[i]) != 0;
  if (!okc) be_raise(vm, "value_error", "colore non valido: servono 6 cifre esadecimali, per esempio ff0000");
  ledScript((int32_t)strtoul(c, NULL, 16));
  be_return_nil(vm);
}

static int bfLedMono(bvm* vm) {
  if (be_top(vm) < 1 || !be_isstring(vm, 1)) be_raise(vm, "type_error", "led1(\"on\"), led1(\"off\") o led1(\"blink\")");
  if (!led2Present()) be_raise(vm, "runtime_error", "questa scheda non ha led1");
  if (!led2SetName(be_tostring(vm, 1))) be_raise(vm, "value_error", "stato non valido: on, off o blink");
  be_return_nil(vm);
}

// Nomi vecchi: l'errore dice il nome nuovo (il messaggio e tradotto)
static int bfLedVecchio(bvm* vm) { be_raise(vm, "runtime_error", ledNomeVecchioMsg("led")); be_return_nil(vm); }
static int bfLed2Vecchio(bvm* vm) { be_raise(vm, "runtime_error", ledNomeVecchioMsg("led2")); be_return_nil(vm); }

void funzioniRegistraLed(bvm* vm, bool rgb, bool mono, bool vecchioLed, bool vecchioLed2) {
  if (rgb && VOS_HAS_LED_RGB) be_regfunc(vm, "led_rgb1", bfLedRgb);
  if (mono && led2Present()) be_regfunc(vm, "led1", bfLedMono);
  if (vecchioLed && ledNomeVecchioMsg("led")) be_regfunc(vm, "led", bfLedVecchio);
  if (vecchioLed2 && ledNomeVecchioMsg("led2")) be_regfunc(vm, "led2", bfLed2Vecchio);
}
#else
void funzioniRegistraLed(bvm*, bool, bool, bool, bool) {}
#endif
