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

#include <string.h>
#include "esp_debug_helpers.h"
#include "esp_private/panic_internal.h"
#include "xtensa_context.h"

extern "C" void __real_esp_panic_handler(panic_info_t* info);

static DrvPanicFn   g_hook = nullptr;
static DrvPanicInfo g_info;          // statica: nel panico lo stack puo essere rovinato
static bool         g_inHook = false; // un secondo panico dentro il gancio non lo richiama

void drvPanicHook(DrvPanicFn fn) {
  g_hook = fn;
}

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

extern "C" void __wrap_esp_panic_handler(panic_info_t* info) {
  if (g_hook != nullptr && !g_inHook && info != nullptr) {
    g_inHook = true;
    memset(&g_info, 0, sizeof(g_info));
    g_info.core   = info->core;
    g_info.reason = (info->reason != nullptr) ? info->reason : info->description;
    g_info.pc     = info->addr;
    if (info->frame != nullptr) {
      fillBacktrace(static_cast<const XtExcFrame*>(info->frame), g_info);
    }
    g_hook(&g_info);
    g_inHook = false;
  }
  __real_esp_panic_handler(info);
}
