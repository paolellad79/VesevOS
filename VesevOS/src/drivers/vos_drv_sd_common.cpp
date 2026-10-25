// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_drv_sd_common.cpp
// STRATO 1 (driver): parti della microSD uguali per ogni backend (idf).
// Riconoscimento del formato dai primi settori e prova di scrittura/lettura.
// Usa solo il contratto (vos_drv_sd.h): niente Arduino, niente esp_*.
#include "vos_drv_sd.h"
#include "../boards/vos_packages.h"
#if VOS_WITH_SD
#include <string.h>
#include <stdlib.h>

static const int SECTOR_SIZE = 512;
static const int MBR_TABLE = 446;       // inizio della tabella delle partizioni nell'MBR
static const int MBR_ENTRY_SIZE = 16;
static const int MBR_ENTRY_COUNT = 4;

// Il settore e fatto tutto di 0x00 oppure tutto di 0xFF?
// Succede con un modulo senza scheda o con il filo MISO scollegato (la linea resta ferma).
static bool isStuckLine(const uint8_t* sector) {
  bool allZero = true;
  bool allOnes = true;

  for (int i = 0; i < SECTOR_SIZE; i++) {
    if (sector[i] != 0x00) {
      allZero = false;
    }
    if (sector[i] != 0xFF) {
      allOnes = false;
    }
  }

  return allZero || allOnes;
}

static bool hasSignature(const uint8_t* sector) {
  return sector[510] == 0x55 && sector[511] == 0xAA;
}

// Il settore 0 e un boot sector (superfloppy) e non un MBR?
static bool looksLikeBootSector(const uint8_t* sector) {
  return sector[0] == 0xEB || sector[0] == 0xE9;
}

static uint32_t readLe32(const uint8_t* p) {
  return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

// Primo settore di una partizione usata nell'MBR (0 = nessuna).
static uint32_t firstPartitionLba(const uint8_t* mbr) {
  for (int i = 0; i < MBR_ENTRY_COUNT; i++) {
    const uint8_t* entry = mbr + MBR_TABLE + MBR_ENTRY_SIZE * i;
    bool used = entry[4] != 0;

    if (used) {
      return readLe32(entry + 8);
    }
  }

  return 0;
}

static uint8_t fsFromBootSector(const uint8_t* b) {
  if (!memcmp(b + 3, "EXFAT   ", 8)) {
    return DFS_EXFAT;
  }
  if (!memcmp(b + 82, "FAT32   ", 8)) {
    return DFS_FAT32;
  }
  if (!memcmp(b + 54, "FAT16   ", 8)) {
    return DFS_FAT16;
  }
  if (!memcmp(b + 54, "FAT12   ", 8)) {
    return DFS_FAT12;
  }

  return DFS_UNKNOWN;
}

void drvSdProbeFs(DrvSdSectorFn read, void* ctx, uint8_t* buf, uint8_t& fs, uint8_t& sector0) {
  fs = DFS_UNKNOWN;
  sector0 = DS0_UNREADABLE;

  if (!read || !buf || !read(ctx, buf, 0)) {
    return;
  }

  if (isStuckLine(buf)) {
    sector0 = DS0_EMPTY;
    return;
  }

  if (!hasSignature(buf)) {
    sector0 = DS0_NO_SIGNATURE;
    return;
  }

  sector0 = DS0_VALID;

  if (!looksLikeBootSector(buf)) {
    uint32_t lba = firstPartitionLba(buf);

    if (lba == 0 || !read(ctx, buf, lba) || !hasSignature(buf)) {
      return;
    }
  }

  fs = fsFromBootSector(buf);
}

// Scrive "bytes" byte in un file temporaneo, li rilegge e li confronta.
// La RAM serve solo durante la prova e si restituisce subito.
uint8_t drvSdProve(uint32_t bytes, uint32_t& writeMs, uint32_t& readMs) {
  writeMs = 0;
  readMs = 0;

  if (!drvSdUp()) {
    return DSP_NOT_UP;
  }

  uint8_t* written = (uint8_t*)malloc(2 * (size_t)bytes);

  if (!written) {
    return DSP_NO_MEM;
  }

  uint8_t* readBack = written + bytes;

  for (uint32_t i = 0; i < bytes; i++) {
    written[i] = (uint8_t)(i * 7 + 13);
  }

  const char* file = "/vesevos-prova.tmp";
  uint32_t t0 = drvSdMs();
  bool wrote = drvSdWrite(file, written, bytes, false);
  uint32_t t1 = drvSdMs();
  int count = wrote ? drvSdRead(file, readBack, bytes) : -1;
  uint32_t t2 = drvSdMs();
  bool same = count == (int)bytes && memcmp(written, readBack, bytes) == 0;

  free(written);
  drvSdRemove(file);

  writeMs = t1 - t0;
  readMs = t2 - t1;

  if (!wrote) {
    return DSP_NO_WRITE;
  }

  return same ? DSP_OK : DSP_DIFFERENT;
}

#endif
