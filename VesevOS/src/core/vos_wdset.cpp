// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_wdset.cpp
// Cambio dei tempi dei watchdog. Un solo punto che tocca esp_task_wdt_reconfigure, dentro un blocco: mai due task insieme.
#include "vos_wdset.h"
#include "vos_wd.h"
#include "vos_config.h"
#include "vos_audit.h"
#include "vos_i18n.h"
#include "vos_lock.h"
#include "../drivers/vos_drv_boot.h"

static VosMutex g_mx;

bool wdCheck(const WdChange& c, ErrBuf& err) {
  if (c.twdt != -1 && !wdTwdtOk(c.twdt)) {
    err.setf(tr("Task watchdog: da %d a %d secondi. Un watchdog non si puo spegnere"), (int)WD_TWDT_MIN, (int)WD_TWDT_MAX);
    return false;
  }
  for (int i = 0; i < WD_LIM_N; i++) {
    if (c.lim[i] != -1 && !wdLimOk(i, c.lim[i])) {
      err.setf(tr("Limite di '%s': da %d a %d secondi. Un watchdog non si puo spegnere"), WD_LIMITS[i].name, (int)WD_LIMITS[i].min, (int)WD_LIMITS[i].max);
      return false;
    }
  }
  return true;
}

static void note(const char* name, int from, int to) {
  auditEvent(AUD_INFO, "", trf("Watchdog '%s' cambiato da %d a %d secondi", name, from, to).c_str());
}

bool wdApply(const WdChange& c, ErrBuf& err) {
  if (!wdCheck(c, err)) return false;
  VosLock lock(g_mx);
  int oldTw = cfg.wdTwdt;
  if (c.twdt != -1 && c.twdt != oldTw) {
    if (!drvTwdtSet((uint32_t)c.twdt)) { err = tr("Il Task watchdog non ha accettato il nuovo tempo, non cambia nulla"); return false; }
    cfg.wdTwdt = (uint8_t)c.twdt;
    note("twdt", oldTw, c.twdt);
  }
  for (int i = 0; i < WD_LIM_N; i++) {
    if (c.lim[i] == -1 || c.lim[i] == cfg.wdLim[i]) continue;
    int old = cfg.wdLim[i];
    cfg.wdLim[i] = (uint16_t)c.lim[i];
    wdRetime(WD_LIMITS[i].name, (uint16_t)old, cfg.wdLim[i]);
    note(WD_LIMITS[i].name, old, c.lim[i]);
  }
  cfgSave();
  return true;
}

bool wdSetOne(const char* name, long value, ErrBuf& err) {
  WdChange c;
  if (value < 0 || value > 100000) { err = tr("Valore fuori campo"); return false; }
  if (name && !strcmp(name, "twdt")) c.twdt = (int)value;
  else {
    int i = wdLimIndex(name);
    if (i < 0) { err = tr("Parametro sconosciuto"); return false; }
    c.lim[i] = (int)value;
  }
  return wdApply(c, err);
}

bool wdDefaults(ErrBuf& err) {
  WdChange c;
  c.twdt = WD_TWDT_DEF;
  for (int i = 0; i < WD_LIM_N; i++) c.lim[i] = WD_LIMITS[i].def;
  return wdApply(c, err);
}

void wdSetInit() {
  g_mx.init();
  if (cfg.wdTwdt != drvTwdtSec()) (void)drvTwdtSet(cfg.wdTwdt);   // il valore di compilazione (30 s) resta se non riesce
}
