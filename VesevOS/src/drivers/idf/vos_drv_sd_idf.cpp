// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_drv_sd_idf.cpp
// STRATO 1 (driver), backend ESP-IDF NATIVO: microSD su SPI con sdspi_host + esp_vfs_fat_sdspi_mount.
// Si compila solo con VOS_BACKEND = VOS_BACKEND_IDF (boards/vos_packages.h). Il contratto sta in ../vos_drv_sd.h.
// STATO: scritto sulla carta, NON ancora provato su una scheda vera (vedi docs/progetto/da-fare-backend-sostituibili.md).
#include "../vos_drv_sd.h"
#include "../../boards/vos_packages.h"
#if VOS_WITH_SD && VOS_BACKEND == VOS_BACKEND_IDF
#include <stdio.h>
#include "../vos_drv_activity.h"
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include "esp_timer.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "sd_protocol_defs.h"
#include "driver/sdspi_host.h"
#include "driver/spi_common.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static const char* MOUNT_POINT = "/sd";
static const spi_host_device_t SPI_HOST = SPI2_HOST;
static const int MAX_OPEN_FILES = 5;
static const int MAX_TRANSFER_BYTES = 4096;
static const uint32_t LOCK_WAIT_MS = 3000;
static const size_t FULL_PATH_MAX = 160;

static sdmmc_card_t* g_card = nullptr;
static bool g_busOwned = false;     // il bus SPI l'abbiamo creato noi (va liberato noi)
static volatile bool g_up = false;
static uint8_t g_fail = DSF_NONE;

// ---- Blocco: le funzioni si possono chiamare da piu task ----

static SemaphoreHandle_t mutex() {
  static SemaphoreHandle_t m = xSemaphoreCreateMutex();
  return m;
}

struct SdLock {
  bool ok;

  SdLock() {
    ok = xSemaphoreTake(mutex(), pdMS_TO_TICKS(LOCK_WAIT_MS)) == pdTRUE;
  }

  ~SdLock() {
    if (ok) {
      xSemaphoreGive(mutex());
    }
  }
};

// ---- Aiuti ----

uint32_t drvSdMs() {
  return (uint32_t)(esp_timer_get_time() / 1000);
}

// "/dir/file" diventa "/sd/dir/file".
static bool fullPath(const char* path, char* out) {
  int n = snprintf(out, FULL_PATH_MAX, "%s%s", MOUNT_POINT, path);
  return n > 0 && (size_t)n < FULL_PATH_MAX;
}

static bool readRawSector(void* ctx, uint8_t* buf, uint32_t lba) {
  sdmmc_card_t* card = (sdmmc_card_t*)ctx;
  return sdmmc_read_sectors(card, buf, lba, 1) == ESP_OK;
}

static void releaseBus() {
  if (g_busOwned) {
    spi_bus_free(SPI_HOST);
    g_busOwned = false;
  }
}

// Dopo un montaggio fallito: la scheda risponde? Che formato ha?
// Il montaggio libera tutto quando fallisce: si riapre la scheda a mano e si legge il settore 0.
static uint8_t whyMountFailed(const sdmmc_host_t& hostTemplate, const sdspi_device_config_t& slot) {
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
    drvSdProbeFs(readRawSector, card, sector, fs, sector0);

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

// ---- Montaggio ----

bool drvSdUp() {
  return g_up;
}

uint8_t drvSdLastFail() {
  return g_fail;
}

static bool openBus(int mosi, int sck, int miso) {
  spi_bus_config_t bus = {};
  bus.mosi_io_num = mosi;
  bus.miso_io_num = miso;
  bus.sclk_io_num = sck;
  bus.quadwp_io_num = -1;
  bus.quadhd_io_num = -1;
  bus.max_transfer_sz = MAX_TRANSFER_BYTES;

  esp_err_t err = spi_bus_initialize(SPI_HOST, &bus, SPI_DMA_CH_AUTO);
  g_busOwned = (err == ESP_OK);

  // ESP_ERR_INVALID_STATE: il bus e gia aperto da qualcun altro, lo usiamo cosi com'e.
  return err == ESP_OK || err == ESP_ERR_INVALID_STATE;
}

bool drvSdBegin(int cs, int mosi, int sck, int miso, uint32_t hz) {
  SdLock lock;

  if (!lock.ok) {
    return false;
  }
  if (g_up) {
    return true;
  }

  g_fail = DSF_NONE;

  if (!openBus(mosi, sck, miso)) {
    g_fail = DSF_NO_CARD;
    return false;
  }

  sdmmc_host_t host = SDSPI_HOST_DEFAULT();
  host.slot = SPI_HOST;
  host.max_freq_khz = (int)(hz / 1000);

  sdspi_device_config_t slot = SDSPI_DEVICE_CONFIG_DEFAULT();
  slot.gpio_cs = (gpio_num_t)cs;
  slot.host_id = SPI_HOST;

  esp_vfs_fat_mount_config_t mount = {};
  mount.format_if_mount_failed = false;    // mai formattare da soli
  mount.max_files = MAX_OPEN_FILES;
  mount.allocation_unit_size = 16 * 1024;

  esp_err_t err = esp_vfs_fat_sdspi_mount(MOUNT_POINT, &host, &slot, &mount, &g_card);

  if (err != ESP_OK) {
    g_card = nullptr;
    g_fail = whyMountFailed(host, slot);
    releaseBus();
    return false;
  }

  g_up = true;
  return true;
}

void drvSdEnd() {
  SdLock lock;

  if (!lock.ok) {
    return;
  }

  g_up = false;

  if (g_card) {
    esp_vfs_fat_sdcard_unmount(MOUNT_POINT, g_card);
    g_card = nullptr;
  }

  releaseBus();
}

// ---- Stato ----

uint8_t drvSdFsType() {
  SdLock lock;

  if (!lock.ok || !g_up || !g_card) {
    return DFS_UNKNOWN;
  }

  uint8_t sector[512];
  uint8_t fs = DFS_UNKNOWN;
  uint8_t sector0 = DS0_UNREADABLE;
  drvSdProbeFs(readRawSector, g_card, sector, fs, sector0);

  return fs;
}

// Chiede lo stato alla scheda: se non risponde e stata tolta, quindi smonta da sola.
bool drvSdAlive() {
  SdLock lock;

  if (!lock.ok) {
    return g_up;    // occupato: non si decide
  }
  if (!g_up || !g_card) {
    return false;
  }

  bool answers = sdmmc_get_status(g_card) == ESP_OK;

  if (!answers) {
    g_up = false;
    esp_vfs_fat_sdcard_unmount(MOUNT_POINT, g_card);
    g_card = nullptr;
    releaseBus();
  }

  return answers;
}

uint8_t drvSdType() {
  SdLock lock;

  if (!lock.ok || !g_up || !g_card) {
    return DSD_NONE;
  }
  if (g_card->is_mmc) {
    return DSD_MMC;
  }

  bool highCapacity = (g_card->ocr & SD_OCR_SDHC_CAP) != 0;
  return highCapacity ? DSD_SDHC : DSD_SD;
}

uint64_t drvSdCardBytes() {
  SdLock lock;

  if (!lock.ok || !g_up || !g_card) {
    return 0;
  }

  return (uint64_t)g_card->csd.capacity * g_card->csd.sector_size;
}

// Spazio del volume FAT: totale e libero.
static bool volumeInfo(uint64_t& total, uint64_t& freeBytes) {
  total = 0;
  freeBytes = 0;

  SdLock lock;

  if (!lock.ok || !g_up) {
    return false;
  }

  return esp_vfs_fat_info(MOUNT_POINT, &total, &freeBytes) == ESP_OK;
}

uint64_t drvSdTotalBytes() {
  uint64_t total = 0;
  uint64_t freeBytes = 0;
  volumeInfo(total, freeBytes);
  return total;
}

uint64_t drvSdUsedBytes() {
  uint64_t total = 0;
  uint64_t freeBytes = 0;

  if (!volumeInfo(total, freeBytes) || freeBytes > total) {
    return 0;
  }

  return total - freeBytes;
}

// ---- File e cartelle ----

int drvSdRead(const char* path, uint8_t* buf, size_t max) {
  char full[FULL_PATH_MAX];
  SdLock lock;

  if (!lock.ok || !g_up || !fullPath(path, full)) {
    return -1;
  }

  FILE* file = fopen(full, "rb");

  if (!file) {
    return -1;
  }

  drvActAdd(ACT_SD_READ);
  size_t count = fread(buf, 1, max, file);
  bool failed = ferror(file) != 0;
  fclose(file);

  return failed ? -1 : (int)count;
}

bool drvSdWrite(const char* path, const uint8_t* buf, size_t n, bool append) {
  char full[FULL_PATH_MAX];
  SdLock lock;

  if (!lock.ok || !g_up || !fullPath(path, full)) {
    return false;
  }

  FILE* file = fopen(full, append ? "ab" : "wb");

  if (!file) {
    return false;
  }

  drvActAdd(ACT_SD_WRITE);
  size_t written = n ? fwrite(buf, 1, n, file) : 0;
  bool closed = fclose(file) == 0;

  return written == n && closed;
}

static void fillEntry(DrvSdEntry& out, const char* folderPath, const dirent& entry) {
  strncpy(out.name, entry.d_name, sizeof(out.name) - 1);
  out.name[sizeof(out.name) - 1] = 0;
  out.dir = entry.d_type == DT_DIR;
  out.size = 0;

  if (!out.dir) {
    char full[FULL_PATH_MAX];
    struct stat info;
    int n = snprintf(full, sizeof full, "%s/%s", folderPath, entry.d_name);

    if (n > 0 && (size_t)n < sizeof full && stat(full, &info) == 0) {
      out.size = (uint32_t)info.st_size;
    }
  }
}

int drvSdList(const char* dir, DrvSdEntry* out, int max, bool& more) {
  more = false;

  char full[FULL_PATH_MAX];
  SdLock lock;

  if (!lock.ok || !g_up || !fullPath(dir, full)) {
    return -1;
  }

  DIR* folder = opendir(full);

  if (!folder) {
    return -1;
  }

  int count = 0;

  for (struct dirent* entry = readdir(folder); entry; entry = readdir(folder)) {
    bool dotEntry = !strcmp(entry->d_name, ".") || !strcmp(entry->d_name, "..");

    if (dotEntry) {
      continue;
    }
    if (count >= max) {
      more = true;
      break;
    }

    fillEntry(out[count], full, *entry);
    count++;
  }

  closedir(folder);
  return count;
}

bool drvSdRemove(const char* path) {
  char full[FULL_PATH_MAX];
  SdLock lock;

  return lock.ok && g_up && fullPath(path, full) && unlink(full) == 0;
}

bool drvSdMkdir(const char* path) {
  char full[FULL_PATH_MAX];
  SdLock lock;

  return lock.ok && g_up && fullPath(path, full) && mkdir(full, 0775) == 0;
}

#endif
