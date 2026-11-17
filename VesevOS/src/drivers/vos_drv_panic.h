// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_drv_panic.h
// STRATO 1 (driver): il gancio che il sistema chiama DENTRO un crash (panico), per salvare un breve rapporto.
// Lo da ESP-IDF tramite il linker (--wrap=esp_panic_handler); il gancio parte per primo, poi il gestore normale stampa e riavvia.
#pragma once
#include <stdint.h>

struct DrvPanicInfo {
  int          core;                 // core che ha avuto il panico
  int          tipo;                 // genere del panico (panic_exception_t di ESP-IDF: 0 debug, 1 interrupt watchdog, 2 task watchdog, 3 abort, 4 eccezione)
  const char*  reason;               // motivo (testo)
  const void*  pc;                   // indirizzo dell'istruzione
  uint32_t     segni;                // bit letti dal gancio: 1 = Task watchdog (g_twdt_isr), 2 = punto di controllo di fine pila (S3)
  const char*  dettagli;             // testo dell'abort (g_panic_abort_details), nullptr se non c'e: puo stare in memoria non copiabile, solo da leggere
  bool         backtraceCorrupt;     // la traccia e rovinata
  bool         backtraceContinues;   // la traccia continua ma non c'e stata
  unsigned int backtraceLen;         // quanti indirizzi nella traccia
  unsigned int backtrace[60];        // gli indirizzi
};

typedef void (*DrvPanicFn)(const DrvPanicInfo* info);

// Installa il gancio. Dentro il gancio: niente memoria dinamica, niente file, niente blocchi.
void drvPanicHook(DrvPanicFn fn);

// LED del codice DENTRO il panico (solo schede con VOS_LED_DIAG_PANICO = 1: C3, C6). Mano diretta sul GPIO del LED semplice: nessun task, timer,
// RMT, memoria dinamica o log. Il ritardo usa la funzione ROM di attesa a cicli (esp_rom_delay_us). Sulle altre schede sono funzioni vuote.
// Il giro lo compone chi chiama (core/vos_crash.cpp, con le regole di vos_diag_rule.h). Durata massima di un giro: circa 4,0 s (DIAG_PANICO_MAX_MS = 4014 ms).
void drvPanicLed(bool acceso);
void drvPanicAttendiMs(uint32_t ms);
