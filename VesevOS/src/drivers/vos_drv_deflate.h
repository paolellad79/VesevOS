// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_drv_deflate.h
// STRATO 1 (driver): pezzi in comune dei comandi di archivio (tar, zip, unzip): il turno unico (DrvDefLock), la guardia di tempo e annullamento
// (DrvDefGuard) e il CRC32 della ROM (esp_rom_crc32_le). La compressione e la decompressione deflate sono state TOLTE (decisione dell'utente del
// 16/11/2026: i comandi devono funzionare uguali su S3, C3 e C6, e il compressore chiedeva la PSRAM). Il nome del file resta per non cambiare gli include.
//
// CHIAMANTE: una sola operazione alla volta (DrvDefLock), e un DrvDefGuard che cede la CPU e controlla tempo e annullamento.
#pragma once
#include <stddef.h>
#include <stdint.h>

enum DrvDefErr : uint8_t {
  DDF_OK = 0,        // tutto bene
  DDF_BAD_DATA,      // dati dell'archivio non validi (letti meno byte di quelli dichiarati)
  DDF_SINK,          // chi riceve i dati ha detto basta (spazio finito, tetto superato...)
  DDF_CANCEL,        // fermato da chi chiama (Ctrl+C)
  DDF_TIMEOUT,       // tetto di tempo superato
};

const size_t   DRV_DEF_YIELD_BYTES = 4096;      // ogni quanti byte trattati si cede la CPU

// Chiamata ogni tanto da DrvDefGuard: true = ferma l'operazione
typedef bool (*DrvDefCancelFn)(void* user);

// Una sola compressione o decompressione alla volta. Si crea sullo stack: ok() dice se il turno e tuo; il turno finisce col distruttore.
class DrvDefLock {
 public:
  DrvDefLock();
  ~DrvDefLock();
  DrvDefLock(const DrvDefLock&) = delete;
  DrvDefLock& operator=(const DrvDefLock&) = delete;
  bool ok() const { return ok_; }
 private:
  bool ok_;
};

// Tetto di tempo e annullamento. tick(n) si chiama per ogni pezzo trattato (n byte): ogni DRV_DEF_YIELD_BYTES cede la CPU
// (drvTaskPause: il watchdog dei task non scatta) e controlla scadenza e annullamento.
class DrvDefGuard {
 public:
  void start(uint32_t maxMs, DrvDefCancelFn cancel, void* user);
  DrvDefErr tick(size_t n);                 // DDF_OK, DDF_TIMEOUT o DDF_CANCEL
  uint32_t elapsedMs() const;
 private:
  uint32_t startMs_ = 0;
  uint32_t maxMs_ = 0;
  size_t   pending_ = 0;
  DrvDefCancelFn cancel_ = nullptr;
  void*    user_ = nullptr;
};

// CRC32 standard (quello di zip). Il primo pezzo parte da crc = 0; i successivi continuano dal valore ritornato.
uint32_t drvCrc32(uint32_t crc, const uint8_t* data, size_t n);
