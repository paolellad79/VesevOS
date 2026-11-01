// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_drv_sd_diag_idf.cpp
// STRATO 1 (driver), backend ESP-IDF: diagnosi della microSD (settore 0, formato, perche non si monta). Usata da vos_drv_sd_idf.cpp.
#include "../vos_drv_sd.h"
#include <stdlib.h>
#include "sdmmc_cmd.h"
#include "driver/sdspi_host.h"

bool drvSdRawSector(void* ctx, uint8_t* buf, uint32_t lba) {
  sdmmc_card_t* card = (sdmmc_card_t*)ctx;
  return sdmmc_read_sectors(card, buf, lba, 1) == ESP_OK;
}

// Dopo un montaggio fallito: la scheda risponde? Che formato ha?
// Il montaggio libera tutto quando fallisce: si riapre la scheda a mano e si legge il settore 0.
uint8_t drvSdWhyMountFailed(const sdmmc_host_t& hostTemplate, const sdspi_device_config_t& slot) {
  sdspi_dev_handle_t handle;

  if (sdspi_host_init_device(&slot, &handle) != ESP_OK) {
    return DSF_NO_CARD;
  }

  sdmmc_host_t host = hostTemplate;
  host.slot = handle;

  sdmmc_card_t* card = (sdmmc_card_t*)malloc(sizeof(sdmmc_card_t));
  uint8_t* sector = (uint8_t*)malloc(512);
  uint8_t why = DSF_NO_CARD;

  if (card && sector && sdmmc_card_init(&host, card) == ESP_OK) {
    uint8_t fs = DFS_UNKNOWN;
    uint8_t sector0 = DS0_UNREADABLE;
    drvSdProbeFs(drvSdRawSector, card, sector, fs, sector0);

    if (sector0 != DS0_VALID) {
      why = DSF_NO_RESPONSE;
    } else {
      why = (fs == DFS_EXFAT) ? DSF_EXFAT : DSF_BAD_FS;
    }
  }

  sdspi_host_remove_device(handle);
  free(card);
  free(sector);

  return why;
}
