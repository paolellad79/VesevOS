// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// VesevOS - vos_pins.h
// Registro dei pin: chi usa quale pin (per la scheda "Pin").
#pragma once
#include <Arduino.h>

struct PinInfo {
  uint8_t gpio;
  const char* owner;   // chi lo usa
  const char* note;    // descrizione
  bool fixed;          // true = riservato dal sistema
};

void   pinsInit();
bool   pinClaim(uint8_t gpio, const char* owner, const char* note, bool fixed = false);
void   pinRelease(uint8_t gpio);
int    pinCount();
const PinInfo* pinAt(int i);
bool   pinIsUsed(uint8_t gpio);
String pinsJson();
