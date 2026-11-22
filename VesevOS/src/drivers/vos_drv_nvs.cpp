// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_drv_nvs.cpp
// NVS con le funzioni native di ESP-IDF. Ogni scrittura fa subito nvs_commit, come faceva Preferences.
#include "vos_drv_nvs.h"
#include <nvs.h>
#include <nvs_flash.h>
#include <esp_log.h>

static const char* TAG = "nvs";
static uint32_t g_errori = 0;

// Esito di una scrittura: se e riuscita si fa il commit; ogni errore si conta e si scrive sulla console (prima si perdeva in silenzio).
static bool esito(nvs_handle_t h, esp_err_t e, const char* k, const char* cosa) {
  if (e == ESP_OK) e = nvs_commit(h);
  if (e == ESP_OK) return true;
  g_errori++;
  ESP_LOGW(TAG, "%s della chiave \"%s\" non riuscita: %s", cosa, k ? k : "?", esp_err_to_name(e));
  return false;
}

uint32_t DrvNvs::errori() { return g_errori; }

bool DrvNvs::begin(const char* ns, bool readOnly) {
  if (open_) return false;
  nvs_handle_t h = 0;
  esp_err_t e = nvs_open(ns, readOnly ? NVS_READONLY : NVS_READWRITE, &h);
  if (e == ESP_ERR_NVS_NOT_INITIALIZED) {            // la NVS non e ancora stata avviata: la avviamo noi
    if (nvs_flash_init() == ESP_OK) e = nvs_open(ns, readOnly ? NVS_READONLY : NVS_READWRITE, &h);
  }
  if (e != ESP_OK) return false;
  h_ = h; ro_ = readOnly; open_ = true;
  return true;
}

void DrvNvs::end() {
  if (!open_) return;
  nvs_close((nvs_handle_t)h_);
  open_ = false;
}

uint32_t DrvNvs::getUInt(const char* k, uint32_t d) {
  uint32_t v = d;
  if (!open_ || nvs_get_u32((nvs_handle_t)h_, k, &v) != ESP_OK) return d;
  return v;
}

uint8_t DrvNvs::getUChar(const char* k, uint8_t d) {
  uint8_t v = d;
  if (!open_ || nvs_get_u8((nvs_handle_t)h_, k, &v) != ESP_OK) return d;
  return v;
}

bool DrvNvs::getBool(const char* k, bool d) { return getUChar(k, d ? 1 : 0) == 1; }

size_t DrvNvs::getString(const char* k, char* out, size_t cap) {
  if (!open_ || !out || !cap) return 0;
  size_t len = 0;
  if (nvs_get_str((nvs_handle_t)h_, k, nullptr, &len) != ESP_OK) return 0;
  if (len == 0 || len > cap) return 0;               // len conta anche il terminatore
  if (nvs_get_str((nvs_handle_t)h_, k, out, &len) != ESP_OK) return 0;
  return len - 1;
}

bool DrvNvs::putUInt(const char* k, uint32_t v) {
  if (!open_ || ro_) return false;
  return esito((nvs_handle_t)h_, nvs_set_u32((nvs_handle_t)h_, k, v), k, "scrittura");
}

bool DrvNvs::putUChar(const char* k, uint8_t v) {
  if (!open_ || ro_) return false;
  return esito((nvs_handle_t)h_, nvs_set_u8((nvs_handle_t)h_, k, v), k, "scrittura");
}

bool DrvNvs::putBool(const char* k, bool v) { return putUChar(k, v ? 1 : 0); }

bool DrvNvs::putString(const char* k, const char* v) {
  if (!open_ || ro_ || !v) return false;
  return esito((nvs_handle_t)h_, nvs_set_str((nvs_handle_t)h_, k, v), k, "scrittura");
}

bool DrvNvs::getBlob(const char* k, void* out, size_t n) {
  if (!open_ || !out || !n) return false;
  size_t len = 0;
  if (nvs_get_blob((nvs_handle_t)h_, k, nullptr, &len) != ESP_OK || len != n) return false;
  return nvs_get_blob((nvs_handle_t)h_, k, out, &len) == ESP_OK;
}

bool DrvNvs::putBlob(const char* k, const void* v, size_t n) {
  if (!open_ || ro_ || !v) return false;
  return esito((nvs_handle_t)h_, nvs_set_blob((nvs_handle_t)h_, k, v, n), k, "scrittura");
}

bool DrvNvs::has(const char* k) {
  if (!open_) return false;
  nvs_type_t t;
  return nvs_find_key((nvs_handle_t)h_, k, &t) == ESP_OK;
}

void DrvNvs::remove(const char* k) {
  if (!open_ || ro_) return;
  if (nvs_erase_key((nvs_handle_t)h_, k) == ESP_OK) nvs_commit((nvs_handle_t)h_);
}

void DrvNvs::clear() {
  if (!open_ || ro_) return;
  if (nvs_erase_all((nvs_handle_t)h_) == ESP_OK) nvs_commit((nvs_handle_t)h_);
}

bool drvNvsAvvia(int& motivoCancellazione) {
  motivoCancellazione = 0;
  esp_err_t e = nvs_flash_init();
  if (e == ESP_ERR_NVS_NO_FREE_PAGES || e == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    motivoCancellazione = (int)e;
    if (nvs_flash_erase() == ESP_OK) e = nvs_flash_init();
  }
  return e == ESP_OK;
}
