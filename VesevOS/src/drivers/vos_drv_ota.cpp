// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_drv_ota.cpp
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "vos_drv_ota.h"
#include <esp_partition.h>
#include <esp_ota_ops.h>
#include <esp_image_format.h>
#include "vos_drv_nvs.h"

void drvOtaConfirmBoot() {
  const esp_partition_t* run = esp_ota_get_running_partition();
  esp_ota_img_states_t state;
  if (run && esp_ota_get_state_partition(run, &state) == ESP_OK && state == ESP_OTA_IMG_PENDING_VERIFY) {
    esp_ota_mark_app_valid_cancel_rollback();
  }
}

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
  DrvNvs p;
  if (!p.begin("vosrec", false)) return false;
  p.putBool("stay", true);
  p.end();
  return esp_partition_erase_range(od, 0, od->size) == ESP_OK;
}

uint32_t drvRecoverySize() { const esp_partition_t* p = recPart(); return p ? (uint32_t)p->size : 0; }

bool drvRecoveryRead(uint32_t off, uint8_t* buf, size_t n) {
  const esp_partition_t* p = recPart();
  return p && off + n <= p->size && esp_partition_read(p, off, buf, n) == ESP_OK;
}

int drvRecoveryWrite(const uint8_t* d, uint32_t n) {
  const esp_partition_t* p = recPart();
  if (!p) return REC_W_NOPART;
  if (n < 4096 || n > p->size) return REC_W_SIZE;
  const esp_partition_t* run = esp_ota_get_running_partition();
  if (!run || run == p) return REC_W_RUNNING;                     // VesevOS deve girare da app0, non dal recovery
  if (esp_ota_get_boot_partition() != run && esp_ota_set_boot_partition(run) != ESP_OK) return REC_W_BOOT;
  const uint32_t SEC = 4096, STEP = 65536;
  uint32_t len = (n + SEC - 1) / SEC * SEC;                       // si cancella solo lo spazio che serve
  for (uint32_t o = 0; o < len; o += STEP) {
    if (esp_partition_erase_range(p, o, (len - o < STEP) ? len - o : STEP) != ESP_OK) return REC_W_ERASE;
    vTaskDelay(1);                                                // lascia respirare gli altri task
  }
  for (uint32_t o = SEC; o < n; o += STEP) {                      // tutto tranne il primo blocco
    if (esp_partition_write(p, o, d + o, (n - o < STEP) ? n - o : STEP) != ESP_OK) return REC_W_WRITE;
    vTaskDelay(1);
  }
  if (esp_partition_write(p, 0, d, SEC) != ESP_OK) return REC_W_WRITE;   // l'intestazione per ultima
  uint8_t b[512];
  for (uint32_t o = 0; o < n; o += sizeof(b)) {
    size_t k = (n - o < sizeof(b)) ? n - o : sizeof(b);
    if (esp_partition_read(p, o, b, k) != ESP_OK || memcmp(b, d + o, k) != 0) return REC_W_VERIFY;
  }
  return REC_W_OK;
}

uint32_t drvSketchSize() {
  const esp_partition_t* run = esp_ota_get_running_partition();
  if (!run) return 0;
  esp_partition_pos_t pos = { run->address, run->size };
  esp_image_metadata_t meta = {};
  meta.start_addr = pos.offset;
  esp_image_verify(ESP_IMAGE_VERIFY, &pos, &meta);
  return meta.image_len;
}

uint32_t drvFreeSketchSpace() {
  const esp_partition_t* next = esp_ota_get_next_update_partition(nullptr);
  return next ? next->size : 0;
}

// Stato dell'immagine come parola semplice (non tradotta: va nel rapporto per l'assistenza).
static const char* imgStateName(esp_ota_img_states_t st) {
  switch (st) {
    case ESP_OTA_IMG_NEW:            return "new";
    case ESP_OTA_IMG_PENDING_VERIFY: return "pending-verify";
    case ESP_OTA_IMG_VALID:          return "valid";
    case ESP_OTA_IMG_INVALID:        return "invalid";
    case ESP_OTA_IMG_ABORTED:        return "aborted";
    default:                         return "undefined";
  }
}

void drvOtaRunningTo(char* out, size_t n) {
  if (!out || !n) return;
  out[0] = 0;
  const esp_partition_t* run = esp_ota_get_running_partition();
  if (!run) return;
  esp_ota_img_states_t st;
  const char* name = (esp_ota_get_state_partition(run, &st) == ESP_OK) ? imgStateName(st) : "n/a";   // senza otadata lo stato non c'e
  snprintf(out, n, "%s @0x%lX, %s", run->label, (unsigned long)run->address, name);
}

void drvOtaNextTo(char* out, size_t n) {
  if (!out || !n) return;
  out[0] = 0;
  const esp_partition_t* nx = esp_ota_get_next_update_partition(nullptr);
  if (nx) snprintf(out, n, "%s @0x%lX, %lu KB", nx->label, (unsigned long)nx->address, (unsigned long)(nx->size / 1024));
}
