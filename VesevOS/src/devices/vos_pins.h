// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_pins.h
// Registro dei pin: chi usa quale pin (per la scheda "Pin").
#pragma once
#include <stdint.h>
#include <string_view>
#include "../core/vos_errbuf.h"
#include "../core/vos_heapbuf.h"

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
// I JSON si scrivono in un HeapBuf del chiamante (tetto PIN_JSON_MAX); false = manca la memoria o supera il tetto.
#define PIN_JSON_MAX 12288
bool   pinsJson(HeapBuf& out);

// ---- Prova dei pin (un solo pin alla volta, si spegne da solo) ----
// Se il pin non si puo provare, restituisce il motivo (gia tradotto); altrimenti nullptr.
const char* pinTestBlock(int gpio);
// action: "high" "low" "blink" "read" "off"; pull (solo per read): "up" "down" "none"
bool   pinTestRequest(int gpio, const char* action, const char* pull, ErrBuf& err);
void   pinTestTick();          // da chiamare spesso (nel loop)
bool   pinTestJson(HeapBuf& out);   // pin in prova, azione, secondi rimasti, livello letto
bool   pinMapJson(HeapBuf& out);    // per ogni GPIO: provabile oppure no, e perche
bool   pinNotesJson(HeapBuf& out);    // inventario: {"5":"sensore porta",...}
bool   pinNoteSet(int gpio, std::string_view name, ErrBuf& err);   // nome libero ("collegato a"), vuoto = cancella
bool   pinBoardJson(HeapBuf& out);  // chip, numero GPIO del chip, nome scheda (rilevamento automatico)
