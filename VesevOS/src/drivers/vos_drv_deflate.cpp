// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_drv_deflate.cpp
// Turno unico, guardia di tempo e CRC32 dei comandi di archivio (vedi vos_drv_deflate.h). Niente miniz: la compressione e stata tolta.
#include "vos_drv_deflate.h"
#include "vos_drv_task.h"
#include "esp_timer.h"
#include "esp_rom_crc.h"
#include "freertos/semphr.h"
#include <string.h>

namespace {

int64_t nowMs() { return esp_timer_get_time() / 1000; }

}  // namespace

// ---------- una operazione alla volta ----------

namespace {
SemaphoreHandle_t busyMutex() {
  static StaticSemaphore_t buf;                                  // creato una sola volta (l'inizializzazione di una static locale e sicura tra task)
  static SemaphoreHandle_t h = xSemaphoreCreateMutexStatic(&buf);
  return h;
}
}  // namespace

DrvDefLock::DrvDefLock() : ok_(xSemaphoreTake(busyMutex(), 0) == pdTRUE) {}

DrvDefLock::~DrvDefLock() {
  if (ok_) xSemaphoreGive(busyMutex());
}

// ---------- guardia: tempo, annullamento, CPU ----------

void DrvDefGuard::start(uint32_t maxMs, DrvDefCancelFn cancel, void* user) {
  startMs_ = (uint32_t)nowMs();
  maxMs_ = maxMs;
  pending_ = 0;
  cancel_ = cancel;
  user_ = user;
}

uint32_t DrvDefGuard::elapsedMs() const { return (uint32_t)nowMs() - startMs_; }

DrvDefErr DrvDefGuard::tick(size_t n) {
  pending_ += n;
  if (pending_ < DRV_DEF_YIELD_BYTES) return DDF_OK;
  pending_ = 0;
  drvTaskPause();                                                // un tick di pausa e il segnale al watchdog: i task di priorita bassa girano
  if (maxMs_ && elapsedMs() > maxMs_) return DDF_TIMEOUT;
  if (cancel_ && cancel_(user_)) return DDF_CANCEL;
  return DDF_OK;
}

// ---------- CRC32 ----------

// esp_rom_crc32_le mette da solo la "~" davanti e dietro (vedi esp_rom_crc.h): con crc = 0 al primo pezzo da' il CRC32 standard.
uint32_t drvCrc32(uint32_t crc, const uint8_t* data, size_t n) {
  return n ? esp_rom_crc32_le(crc, data, (uint32_t)n) : crc;
}
