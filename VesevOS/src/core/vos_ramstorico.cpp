// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_ramstorico.cpp (vedi vos_ramstorico.h)
#include "vos_ramstorico.h"
#include <string.h>
#include "../drivers/vos_drv_task.h"   // porta con se FreeRTOS (portMUX)
#include "../drivers/vos_drv_time.h"
#include "../drivers/vos_drv_heap.h"

static RamNote g_notes[RAM_NOTES];
static uint8_t g_noteN = 0;                  // quante voci valide (max RAM_NOTES)
static uint8_t g_noteHead = 0;               // dove scrivere la prossima
static portMUX_TYPE g_noteMux = portMUX_INITIALIZER_UNLOCKED;

void ramNote(const char* label) {
  uint32_t f = drvHeapFree(), b = drvHeapMaxBlock(), p = drvPsramFree();
  portENTER_CRITICAL(&g_noteMux);
  RamNote& n = g_notes[g_noteHead];
  n.sec = drvMillis() / 1000UL; n.free_ = f; n.big = b; n.ps = p;
  strncpy(n.label, label ? label : "", sizeof(n.label) - 1); n.label[sizeof(n.label) - 1] = 0;
  g_noteHead = (g_noteHead + 1) % RAM_NOTES; if (g_noteN < RAM_NOTES) g_noteN++;
  portEXIT_CRITICAL(&g_noteMux);
}

uint8_t ramNoteCount() { return g_noteN; }

bool ramNoteGet(uint8_t i, RamNote& out) {
  bool ok = false;
  portENTER_CRITICAL(&g_noteMux);
  if (i < g_noteN) { out = g_notes[(g_noteHead + RAM_NOTES - g_noteN + i) % RAM_NOTES]; ok = true; }
  portEXIT_CRITICAL(&g_noteMux);
  return ok;
}
