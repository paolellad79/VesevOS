// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_drv_panic.cpp (strato 1: gancio del panico, vedi vos_drv_panic.h)
// Scritto da zero sopra le API pubbliche di ESP-IDF (Apache-2.0): il tipo del panico (panic_info_t), il frame di eccezione
// Xtensa (XtExcFrame) e il percorso dei frame (esp_backtrace_get_next_frame). Nessun codice del core Arduino.
//
// Come funziona: il linker manda ogni chiamata a esp_panic_handler() a __wrap_esp_panic_handler() (opzione --wrap nel CMake).
// Qui si copia il motivo e la traccia in una struttura statica, si chiama il gancio dell'applicazione e SOLO DOPO si passa
// al gestore vero (__real_esp_panic_handler), che stampa il rapporto sulla console e riavvia.
// Dentro il panico: niente memoria dinamica, niente file, niente blocchi.
#include "vos_drv_panic.h"
#include "../boards/vos_board.h"

#include <string.h>
#include "esp_debug_helpers.h"
#include "esp_private/panic_internal.h"
#if __XTENSA__
#include "xtensa_context.h"
#endif
#if VOS_LED_DIAG_PANICO
#include "esp_rom_gpio.h"
#include "esp_rom_sys.h"
#include "hal/gpio_ll.h"
#endif

extern "C" void __real_esp_panic_handler(panic_info_t* info);
// Flag di task_wdt.c (IDF 5.5.5): vero SOLO dopo task_wdt_timeout_abort(). Simbolo interno, non dichiarato in un header pubblico: referenza
// debole, cosi se un'altra versione lo togliesse il collegamento non si rompe (il codice del LED tornerebbe al generico).
extern "C" bool g_twdt_isr __attribute__((weak));

static DrvPanicFn   g_hook = nullptr;
static DrvPanicInfo g_info;          // statica: nel panico lo stack puo essere rovinato
static bool         g_inHook = false; // un secondo panico dentro il gancio non lo richiama

void drvPanicHook(DrvPanicFn fn) {
  g_hook = fn;
}

#if __XTENSA__
// Riempie la traccia: prima il punto del guasto, poi i chiamanti uno a uno.
// backtraceCorrupt: un passo ha dato un frame non valido. backtraceContinues: la traccia e stata tagliata dal limite.
static void fillBacktrace(const XtExcFrame* frame, DrvPanicInfo& out) {
  const unsigned maxLen = sizeof(out.backtrace) / sizeof(out.backtrace[0]);

  esp_backtrace_frame_t walk;
  walk.pc        = frame->pc;
  walk.sp        = frame->a1;
  walk.next_pc   = frame->a0;
  walk.exc_frame = frame;

  out.backtrace[out.backtraceLen++] = walk.pc;

  while (walk.next_pc != 0 && out.backtraceLen < maxLen) {
    if (!esp_backtrace_get_next_frame(&walk)) {
      out.backtraceCorrupt = true;
      return;
    }
    out.backtrace[out.backtraceLen++] = walk.pc;
  }
  out.backtraceContinues = (walk.next_pc != 0);
}
#endif

#if VOS_LED_DIAG_PANICO
// Il pin del LED e gia assegnato al PWM (LEDC) dal normale driver: qui lo si riprende come semplice uscita. Ordine: scelta del GPIO sul pad,
// collegamento al segnale "GPIO" della matrice (cosi il PWM lo molla), uscita abilitata, livello. Tutte funzioni ROM o registri: nessun blocco.
void drvPanicLed(bool acceso) {
  const uint32_t pin = VOS_PIN_LED2;
  esp_rom_gpio_pad_select_gpio(pin);
  esp_rom_gpio_connect_out_signal(pin, SIG_GPIO_OUT_IDX, false, false);
  gpio_ll_output_enable(&GPIO, pin);
  gpio_ll_set_level(&GPIO, pin, (acceso != (VOS_LED2_ACTIVE_LOW != 0)) ? 1 : 0);
}

void drvPanicAttendiMs(uint32_t ms) {
  while (ms > 0) {
    uint32_t passo = ms > 100 ? 100 : ms;      // pezzi da 100 ms: nessun ritardo ROM troppo lungo in un colpo solo
    esp_rom_delay_us(passo * 1000u);
    ms -= passo;
  }
}
#else
void drvPanicLed(bool) {}
void drvPanicAttendiMs(uint32_t) {}
#endif

extern "C" void __wrap_esp_panic_handler(panic_info_t* info) {
  if (g_hook != nullptr && !g_inHook && info != nullptr) {
    g_inHook = true;
    memset(&g_info, 0, sizeof(g_info));
    g_info.core   = info->core;
    g_info.tipo   = (int)info->exception;
    g_info.reason = (info->reason != nullptr) ? info->reason : info->description;
    g_info.pc     = info->addr;
    if (&g_twdt_isr != nullptr && g_twdt_isr) g_info.segni |= 1u;
    g_info.dettagli = g_panic_abort ? g_panic_abort_details : nullptr;    // abort, assert, pila FreeRTOS, Task watchdog (senza testo)
#if __XTENSA__
    if (info->exception == PANIC_EXCEPTION_DEBUG) {                       // come print_debug_exception_details: DBREAK con il bit 8 = punto di controllo 1 = fine pila
      int dc;
      asm("rsr.debugcause %0" : "=r"(dc));
      if ((dc & 0x4) && (dc & (1 << 8))) g_info.segni |= 2u;
    }
    if (info->frame != nullptr) {
      fillBacktrace(static_cast<const XtExcFrame*>(info->frame), g_info);
    }
#endif  // sul C6 (RISC-V) niente traccia: solo motivo e pc
    g_hook(&g_info);
    g_inHook = false;
  }
  __real_esp_panic_handler(info);
}
