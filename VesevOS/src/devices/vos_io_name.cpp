// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_io_name.cpp
// Nomi delle periferiche a classi (rele1, luce...): forma, parole riservate e pulizia. Spostato qui da vos_io.cpp (file sotto le 500 righe).
// Nessuna String: i nomi stanno in buffer del chiamante (12 byte). Comportamento uguale a quello di prima.
#include "vos_io.h"
#include "vos_io_int.h"
#include "vos_dev.h"
#include "../core/vos_i18n.h"
#include <ctype.h>
#include <string.h>

bool ioNameReserved(const char* n) {
  static const char* const R[] = {"led", "time", "temp", "log", "act", "btn", "logread", "files", "mqtt", "mesh", "ble", "mfa", "stat", "pin", "boot", "usb", "mem", "cpu", "rad", "gpio", "pwm", "all", "tutto", "tutti"};
  for (size_t i = 0; i < sizeof(R) / sizeof(R[0]); i++) if (!strcmp(n, R[i])) return true;
  return devFind(n) != nullptr;
}

// "led_rgb" + cifre (led_rgb2...): unico nome con il trattino basso, quello della classe dei LED a colori (decisione dell'utente del 13/11/2026)
static bool ledRgbNome(const char* n) {
  if (strncmp(n, "led_rgb", 7) != 0 || !n[7]) return false;
  for (const char* p = n + 7; *p; p++) if (*p < '0' || *p > '9') return false;
  return true;
}

const char* ioNameCheck(const char* n) {
  size_t len = strlen(n);
  if (len < 2 || len > IO_NAME_MAX) return tr("Nome: da 2 a 11 caratteri");
  bool ledRgb = ledRgbNome(n);
  for (size_t i = 0; i < len; i++) {
    char c = n[i];
    bool ok = ledRgb || (c >= 'a' && c <= 'z') || (i > 0 && c >= '0' && c <= '9');
    if (!ok) return tr("Nome: solo lettere minuscole senza accento e numeri, e deve iniziare con una lettera");
  }
  if (ioNameReserved(n)) return tr("Nome riservato: scegline un altro");
  return nullptr;
}

// Come String::trim() + toLowerCase(): spazi bianchi (isspace) tolti ai lati, A-Z in minuscolo. La lunghezza si controlla PRIMA di copiare.
const char* ioNameNorm(const char* in, char* out) {
  while (*in && isspace((unsigned char)*in)) in++;
  size_t n = strlen(in);
  while (n && isspace((unsigned char)in[n - 1])) n--;
  if (n > IO_NAME_MAX) { out[0] = 0; return tr("Nome: da 2 a 11 caratteri"); }
  for (size_t i = 0; i < n; i++) out[i] = (in[i] >= 'A' && in[i] <= 'Z') ? (char)(in[i] + ('a' - 'A')) : in[i];
  out[n] = 0;
  return nullptr;
}
