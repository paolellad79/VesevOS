// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_sd_fmt.cpp: la formattazione della microSD gira in un task a parte (puo durare minuti): shell, pagina e programmi
// tornano subito e guardano lo stato con sdFormatInfo(). Una sola formattazione alla volta. Finche dura, montaggio, smontaggio e file
// della scheda rispondono "microSD occupata (formattazione)" e il montaggio automatico sta fermo.
// La formattazione vera e sdFormat (vos_sd.cpp). Il tempo e quello trascorso: il driver non dice a che punto e (nessuna percentuale).
#include "vos_sd.h"

#if VOS_WITH_SD
#include <stdio.h>
#include <string.h>
#include "vos_sd_buf.h"
#include "../core/vos_audit.h"
#include "../core/vos_i18n.h"
#include "../core/vos_log.h"
#include "../drivers/vos_drv_task.h"
#include "../drivers/vos_drv_time.h"

#define SDFMT_STACK 8192
#define SDFMT_PRIO  1               // come il sondaggio: web, shell e rete non aspettano mai la formattazione

enum : uint8_t { FMT_NEVER = 0, FMT_OK = 1, FMT_FAIL = 2 };

static volatile uint8_t       g_run = 0;
static volatile unsigned long g_t0 = 0;
static volatile uint8_t       g_last = FMT_NEVER;
static volatile uint32_t      g_lastSecs = 0;
static char                   g_lastErr[SD_ERR_MAX];

static uint32_t elapsedSecs() { return (uint32_t)((drvMillis() - g_t0) / 1000UL); }

bool sdFormatting() { return g_run != 0; }

const char* sdBusyText() { return g_run ? tr("microSD occupata (formattazione)") : tr("Operazione gia in corso"); }

void sdFormatInfo(SdFmtInfo& f) {
  memset(&f, 0, sizeof f);
  f.running = g_run != 0;
  f.secs = f.running ? elapsedSecs() : 0;
  f.last = g_last;
  f.lastSecs = g_lastSecs;
  snprintf(f.err, sizeof f.err, "%s", g_lastErr);
}

// Il lavoro: formatta, scrive come e finita (registro, allarme se fallisce) e libera il posto per la prossima.
static void sdFormatTask(void*) {
  char err[SD_ERR_MAX];
  err[0] = 0;
  bool ok = sdFormat(err, sizeof err);
  uint32_t secs = elapsedSecs();

  snprintf(g_lastErr, sizeof g_lastErr, "%s", ok ? "" : err);
  g_lastSecs = secs;
  g_last = ok ? FMT_OK : FMT_FAIL;
  if (ok) {
    auditClear("sdfmt");
    auditEvent(AUD_INFO, "sdfmt", tr("microSD formattata in FAT32"));
  } else {
    auditEvent(AUD_YELLOW, "sdfmt", trf("Formattazione della microSD non riuscita: %s", err).c_str());
  }
  vlog("SD: formattazione %s dopo %u s", ok ? "finita" : "non riuscita", (unsigned)secs);
  g_run = 0;
  vTaskDelete(nullptr);
}

bool sdFormatStart(char* err, size_t en) {
  if (!sdConfigured()) {
    sdSetErr(err, en, tr("Nessuna microSD configurata: aggiungila da Periferiche"));
    return false;
  }
  if (__sync_lock_test_and_set(&g_run, 1)) {
    sdSetErr(err, en, tr("microSD occupata (formattazione)"));
    return false;
  }
  if (sdInUse()) {
    g_run = 0;
    sdSetErr(err, en, tr("La microSD e in uso: riprova tra un momento"));
    return false;
  }
  g_t0 = drvMillis();
  if (!drvTaskCreate(sdFormatTask, "sdfmt", SDFMT_STACK, nullptr, SDFMT_PRIO, nullptr, DRV_CORE_ANY)) {
    g_run = 0;
    sdSetErr(err, en, tr("Memoria insufficiente"));
    return false;
  }
  return true;
}
#endif
