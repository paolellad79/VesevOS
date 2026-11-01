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
#include <time.h>
#include <unistd.h>
#include "esp_timer.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "sd_protocol_defs.h"
#include "driver/sdspi_host.h"
#include "driver/spi_common.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

// Punto di montaggio: lo sceglie boards/vos_packages.h (oggi "/media/sd"). Il limite di ESP-IDF per il percorso base e di 15 caratteri
// (ESP_VFS_PATH_MAX): "/media/sd" ne ha 9. Le scorciatoie di input (come "/sd") non sono cosa del driver: stanno in core/vos_paths.
static const char* MOUNT_POINT = VOS_SD_MOUNT_POINT;
static_assert(sizeof(VOS_SD_MOUNT_POINT) - 1 <= 15, "il percorso base dei file system virtuali di ESP-IDF e al massimo di 15 caratteri");
static const spi_host_device_t SPI_HOST = SPI2_HOST;
static const int MAX_OPEN_FILES = 5;
static const int MAX_TRANSFER_BYTES = 4096;
static const uint32_t LOCK_WAIT_MS = 3000;
static const size_t FULL_PATH_MAX = 160;

// Diagnosi della scheda (vos_drv_sd_diag_idf.cpp)
bool drvSdRawSector(void* ctx, uint8_t* buf, uint32_t lba);
uint8_t drvSdWhyMountFailed(const sdmmc_host_t& hostTemplate, const sdspi_device_config_t& slot);

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

// "/dir/file" diventa "/media/sd/dir/file".
static bool fullPath(const char* path, char* out) {
  int n = snprintf(out, FULL_PATH_MAX, "%s%s", MOUNT_POINT, path);
  return n > 0 && (size_t)n < FULL_PATH_MAX;
}


static void releaseBus() {
  if (g_busOwned) {
    spi_bus_free(SPI_HOST);
    g_busOwned = false;
  }
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

// Monta sui 4 pin. fmt = se non si monta (exFAT, vuota, non FAT) formatta e monta (solo per drvSdFormat). Con il blocco preso.
static esp_err_t mountCard(int cs, int mosi, int sck, int miso, uint32_t hz, bool fmt, sdmmc_host_t& host, sdspi_device_config_t& slot) {
  if (!openBus(mosi, sck, miso)) {
    return ESP_ERR_NOT_SUPPORTED;            // il bus SPI non si apre
  }

  host = SDSPI_HOST_DEFAULT();
  host.slot = SPI_HOST;
  host.max_freq_khz = (int)(hz / 1000);
  slot = SDSPI_DEVICE_CONFIG_DEFAULT();
  slot.gpio_cs = (gpio_num_t)cs;
  slot.host_id = SPI_HOST;

  esp_vfs_fat_mount_config_t mount = {};
  mount.format_if_mount_failed = fmt;       // false = mai formattare da soli
  mount.max_files = MAX_OPEN_FILES;
  mount.allocation_unit_size = 16 * 1024;
  return esp_vfs_fat_sdspi_mount(MOUNT_POINT, &host, &slot, &mount, &g_card);
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
  sdmmc_host_t host;
  sdspi_device_config_t slot;

  esp_err_t err = mountCard(cs, mosi, sck, miso, hz, false, host, slot);

  if (err != ESP_OK) {
    g_card = nullptr;
    g_fail = err == ESP_ERR_NOT_SUPPORTED ? (uint8_t)DSF_NO_CARD : drvSdWhyMountFailed(host, slot);
    releaseBus();
    return false;
  }

  g_up = true;
  return true;
}

uint8_t drvSdFormat(int cs, int mosi, int sck, int miso, uint32_t hz) {
  SdLock lock;

  if (!lock.ok) {
    return DSFM_BUSY;
  }
  if (g_up) {
    return DSFM_BUSY;
  }

  sdmmc_host_t host;
  sdspi_device_config_t slot;
  esp_err_t err = mountCard(cs, mosi, sck, miso, hz, false, host, slot);   // gia FAT: si monta e si rifa il volume

  if (err == ESP_OK) {
    err = esp_vfs_fat_sdcard_format(MOUNT_POINT, g_card);
  } else {
    g_card = nullptr;
    releaseBus();                                                            // il bus aperto dal primo tentativo si riapre dal secondo
    err = mountCard(cs, mosi, sck, miso, hz, true, host, slot);              // non si monta: lo formatta il montaggio stesso
  }

  if (err != ESP_OK) {
    if (g_card) {
      esp_vfs_fat_sdcard_unmount(MOUNT_POINT, g_card);
      g_card = nullptr;
    }
    releaseBus();
    return (err == ESP_ERR_NOT_SUPPORTED || err == ESP_ERR_TIMEOUT) ? DSFM_NO_CARD : DSFM_FAIL;
  }

  g_fail = DSF_NONE;
  g_up = true;
  return DSFM_OK;
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
  drvSdProbeFs(drvSdRawSector, g_card, sector, fs, sector0);

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

int drvSdReadAt(const char* path, uint32_t off, uint8_t* buf, size_t max) {
  char full[FULL_PATH_MAX];
  SdLock lock;

  if (!lock.ok || !g_up || !fullPath(path, full)) {
    return -1;
  }

  FILE* file = fopen(full, "rb");

  if (!file) {
    return -1;
  }

  if (fseek(file, (long)off, SEEK_SET) != 0) {
    fclose(file);
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

// Una data di file FAT vale solo se e dopo il 2000: il valore di partenza dei file senza data e il 1980. 0 = non valida.
static uint32_t validMtime(time_t t) {
  const time_t YEAR_2000 = 946684800;
  return t >= YEAR_2000 ? (uint32_t)t : 0;
}

static void fillEntry(DrvSdEntry& out, const char* folderPath, const dirent& entry) {
  strncpy(out.name, entry.d_name, sizeof(out.name) - 1);
  out.name[sizeof(out.name) - 1] = 0;
  out.dir = entry.d_type == DT_DIR;
  out.size = 0;
  out.mtime = 0;

  char full[FULL_PATH_MAX];
  struct stat info;
  int n = snprintf(full, sizeof full, "%s/%s", folderPath, entry.d_name);

  if (n > 0 && (size_t)n < sizeof full && stat(full, &info) == 0) {
    out.size = out.dir ? 0 : (uint32_t)info.st_size;
    out.mtime = validMtime(info.st_mtime);
  }
}

int drvSdListFrom(const char* dir, int start, DrvSdEntry* out, int max, bool& more) {
  more = false;

  char full[FULL_PATH_MAX];
  SdLock lock;

  if (start < 0 || !lock.ok || !g_up || !fullPath(dir, full)) {
    return -1;
  }

  DIR* folder = opendir(full);

  if (!folder) {
    return -1;
  }

  int count = 0;
  int skipped = 0;

  for (struct dirent* entry = readdir(folder); entry; entry = readdir(folder)) {
    bool dotEntry = !strcmp(entry->d_name, ".") || !strcmp(entry->d_name, "..");

    if (dotEntry) {
      continue;
    }
    if (skipped < start) {
      skipped++;
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

int drvSdList(const char* dir, DrvSdEntry* out, int max, bool& more) {
  return drvSdListFrom(dir, 0, out, max, more);
}

bool drvSdStat(const char* path, DrvSdEntry& out) {
  char full[FULL_PATH_MAX];
  SdLock lock;

  if (!lock.ok || !g_up || !fullPath(path, full)) {
    return false;
  }

  struct stat info;

  if (stat(full, &info) != 0) {
    return false;
  }

  const char* slash = strrchr(path, '/');
  const char* name = slash ? slash + 1 : path;

  strncpy(out.name, name, sizeof(out.name) - 1);
  out.name[sizeof(out.name) - 1] = 0;
  out.dir = S_ISDIR(info.st_mode);
  out.size = out.dir ? 0 : (uint32_t)info.st_size;
  out.mtime = validMtime(info.st_mtime);
  return true;
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
