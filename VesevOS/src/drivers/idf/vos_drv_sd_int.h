// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_drv_sd_int.h
// Tra i file del driver della microSD: il blocco, il percorso completo e la chiusura del file a flusso.
#pragma once
#include <stddef.h>
#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

constexpr uint32_t SD_LOCK_ATTESA_MS = 3000;
constexpr size_t SD_PERCORSO_MAX = 160;

SemaphoreHandle_t drvSdMutex();
bool drvSdPercorso(const char* path, char* out);   // "/dir/file" diventa "/media/sd/dir/file"; out da SD_PERCORSO_MAX byte
void drvSdFlussoPerso();                           // da chiamare con il blocco preso, prima di smontare

// Le funzioni si possono chiamare da piu task.
struct SdLock {
  bool ok;

  SdLock() {
    ok = xSemaphoreTake(drvSdMutex(), pdMS_TO_TICKS(SD_LOCK_ATTESA_MS)) == pdTRUE;
  }

  ~SdLock() {
    if (ok) {
      xSemaphoreGive(drvSdMutex());
    }
  }
};
