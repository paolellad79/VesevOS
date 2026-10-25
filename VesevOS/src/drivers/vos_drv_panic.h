// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_drv_panic.h
// STRATO 1 (driver): il gancio che il sistema chiama DENTRO un crash (panico), per salvare un breve rapporto.
// Lo da ESP-IDF tramite il linker (--wrap=esp_panic_handler); il gancio parte per primo, poi il gestore normale stampa e riavvia.
#pragma once

struct DrvPanicInfo {
  int          core;                 // core che ha avuto il panico
  const char*  reason;               // motivo (testo)
  const void*  pc;                   // indirizzo dell'istruzione
  bool         backtraceCorrupt;     // la traccia e rovinata
  bool         backtraceContinues;   // la traccia continua ma non c'e stata
  unsigned int backtraceLen;         // quanti indirizzi nella traccia
  unsigned int backtrace[60];        // gli indirizzi
};

typedef void (*DrvPanicFn)(const DrvPanicInfo* info);

// Installa il gancio. Dentro il gancio: niente memoria dinamica, niente file, niente blocchi.
void drvPanicHook(DrvPanicFn fn);
