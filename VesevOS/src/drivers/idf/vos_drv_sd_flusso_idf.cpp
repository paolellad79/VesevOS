// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_drv_sd_flusso_idf.cpp (vedi vos_drv_sd.h)
// Un file grande scritto a blocchi restando aperto: riaprirlo a ogni blocco costa troppo su una scheda lenta.
#include "../vos_drv_sd.h"
#include "vos_drv_sd_int.h"
#include <stdio.h>
#include "../vos_drv_activity.h"

static FILE* g_flusso = nullptr;

bool drvSdFlussoApri(const char* path) {
  char full[SD_PERCORSO_MAX];
  SdLock lock;

  if (!lock.ok || !drvSdUp() || !drvSdPercorso(path, full)) {
    return false;
  }

  drvSdFlussoPerso();                              // resto di un lavoro che non ha potuto chiudere
  g_flusso = fopen(full, "wb");
  return g_flusso != nullptr;
}

bool drvSdFlussoScrivi(const uint8_t* buf, size_t n) {
  SdLock lock;

  if (!lock.ok || !g_flusso) {
    return false;
  }

  drvActAdd(ACT_SD_WRITE);
  return fwrite(buf, 1, n, g_flusso) == n;
}

bool drvSdFlussoChiudi() {
  SdLock lock;

  if (!lock.ok || !g_flusso) {
    return false;
  }

  bool chiuso = fclose(g_flusso) == 0;
  g_flusso = nullptr;
  return chiuso;
}

void drvSdFlussoPerso() {
  if (g_flusso) {
    fclose(g_flusso);
    g_flusso = nullptr;
  }
}
