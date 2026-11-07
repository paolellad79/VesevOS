// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_apwin.cpp (vedi vos_apwin.h)
#include "vos_apwin.h"
#include "../drivers/vos_drv_time.h"

#define APWIN_MS 600000UL                 // 10 minuti

static volatile bool     g_open  = false;
static volatile uint32_t g_until = 0;

void apWinOpen() {
  g_until = drvMillis() + APWIN_MS;
  g_open = true;
}

bool apWinIsOpen() { return g_open; }

void apWinClose() { g_open = false; }

bool apWinTick() {
  if (!g_open) return false;
  if ((int32_t)(g_until - drvMillis()) > 0) return false;   // ancora nel tempo
  g_open = false;
  return true;
}
