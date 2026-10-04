// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
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

// ---- Prova dei pin (un solo pin alla volta, si spegne da solo) ----
// Se il pin non si puo provare, restituisce il motivo (gia tradotto); altrimenti nullptr.
const char* pinTestBlock(int gpio);
// action: "high" "low" "blink" "read" "off"; pull (solo per read): "up" "down" "none"
bool   pinTestRequest(int gpio, const String& action, const String& pull, String& err);
void   pinTestTick();          // da chiamare spesso (nel loop)
String pinTestJson();          // pin in prova, azione, secondi rimasti, livello letto
String pinMapJson();           // per ogni GPIO: provabile oppure no, e perche
String pinNotesJson();           // inventario: {"5":"sensore porta",...}
bool   pinNoteSet(int gpio, const String& name, String& err);   // nome libero ("collegato a"), vuoto = cancella
String pinNotesJson();           // inventario: {"5":"sensore porta",...}
bool   pinNoteSet(int gpio, const String& name, String& err);   // nome libero ("collegato a"), vuoto = cancella
String pinBoardJson();         // chip, numero GPIO del chip, nome scheda (rilevamento automatico)
