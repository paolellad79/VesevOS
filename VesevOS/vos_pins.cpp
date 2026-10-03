// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// VesevOS - vos_pins.cpp
#include "vos_i18n.h"
#include "vos_pins.h"
#include "vos_util.h"

#define MAX_PINS 24
static PinInfo g_pins[MAX_PINS];
static int g_n = 0;

void pinsInit() {
  g_n = 0;
  pinClaim(19, "USB", "USB D-  (nativo)", true);
  pinClaim(20, "USB", "USB D+  (nativo)", true);
}

bool pinClaim(uint8_t gpio, const char* owner, const char* note, bool fixed) {
  for (int i = 0; i < g_n; i++) {
    if (g_pins[i].gpio == gpio) {
      if (strcmp(g_pins[i].owner, owner) == 0) { g_pins[i].note = note; return true; }
      return false;   // gia usato da altri
    }
  }
  if (g_n >= MAX_PINS) return false;
  g_pins[g_n++] = { gpio, owner, note, fixed };
  return true;
}

void pinRelease(uint8_t gpio) {
  for (int i = 0; i < g_n; i++) {
    if (g_pins[i].gpio == gpio && !g_pins[i].fixed) {
      for (int k = i; k < g_n - 1; k++) g_pins[k] = g_pins[k + 1];
      g_n--; return;
    }
  }
}

int pinCount() { return g_n; }
const PinInfo* pinAt(int i) { return (i >= 0 && i < g_n) ? &g_pins[i] : nullptr; }

bool pinIsUsed(uint8_t gpio) {
  for (int i = 0; i < g_n; i++) if (g_pins[i].gpio == gpio) return true;
  return false;
}

String pinsJson() {
  String j = "[";
  for (int i = 0; i < g_n; i++) {
    if (i) j += ",";
    j += "{\"gpio\":" + String(g_pins[i].gpio) +
         ",\"owner\":\"" + jsonEscape(tr(g_pins[i].owner)) +
         "\",\"note\":\"" + jsonEscape(tr(g_pins[i].note)) +
         "\",\"fixed\":" + (g_pins[i].fixed ? "true" : "false") + "}";
  }
  j += "]";
  return j;
}
