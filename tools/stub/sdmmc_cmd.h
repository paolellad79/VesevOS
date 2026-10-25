#pragma once
// STUB di sdmmc_cmd.h / sdmmc_types.h (solo quello che usa il backend IDF della microSD, per il controllo della sintassi su PC)
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "esp_err.h"
#define SD_OCR_SDHC_CAP (1u << 30)
typedef struct { uint32_t capacity; uint32_t sector_size; } sdmmc_csd_t;
typedef struct { uint32_t ocr; sdmmc_csd_t csd; uint32_t is_mmc; } sdmmc_card_t;
typedef struct { int slot; int max_freq_khz; } sdmmc_host_t;
esp_err_t sdmmc_read_sectors(sdmmc_card_t*, void*, size_t, size_t);
esp_err_t sdmmc_card_init(const sdmmc_host_t*, sdmmc_card_t*);
esp_err_t sdmmc_get_status(sdmmc_card_t*);
