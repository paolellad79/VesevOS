// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_drv_ota.cpp
#include "vos_drv_ota.h"
#include <esp_partition.h>
#include <Preferences.h>

static const esp_partition_t* recPart() {
  return esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_FACTORY, NULL);
}

bool drvRecoveryPresent() {
  const esp_partition_t* p = recPart();
  if (!p) return false;
  uint8_t b = 0;
  return esp_partition_read(p, 0, &b, 1) == ESP_OK && b == 0xE9;   // 0xE9 = inizio di un'immagine ESP
}

uint32_t drvRecoverySizeKB() { const esp_partition_t* p = recPart(); return p ? (uint32_t)(p->size / 1024) : 0; }

// Senza dati "otadata" il bootloader sceglie la partizione factory (il recovery).
// In piu lasciamo un segno in NVS ("vosrec"/"stay"): il recovery lo legge e RESTA li invece di tornare subito a VesevOS.
bool drvRecoveryEnter() {
  if (!drvRecoveryPresent()) return false;
  const esp_partition_t* od = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_OTA, NULL);
  if (!od) return false;
  Preferences p;
  if (!p.begin("vosrec", false)) return false;
  p.putBool("stay", true);
  p.end();
  return esp_partition_erase_range(od, 0, od->size) == ESP_OK;
}
