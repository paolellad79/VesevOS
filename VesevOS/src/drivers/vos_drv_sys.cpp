// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_drv_sys.cpp
#include "vos_drv_sys.h"
#include <math.h>
#include <stdio.h>
#include <esp_system.h>
#include <esp_chip_info.h>
#include <esp_flash.h>
#include <driver/temperature_sensor.h>
#include <esp_app_desc.h>

void drvSysRestart() { esp_restart(); }

// Nome costruito dal modello letto dal chip ("ESP32-" + serie): vale per ogni scheda, nessun nome fisso
const char* drvChipModel() {
  static char name[16];
  esp_chip_info_t ci;
  esp_chip_info(&ci);
  const char* serie = "-?";
  switch (ci.model) {
    case CHIP_ESP32S3: serie = "-S3"; break;
    case CHIP_ESP32S2: serie = "-S2"; break;
    case CHIP_ESP32C3: serie = "-C3"; break;
    case CHIP_ESP32C6: serie = "-C6"; break;
    case CHIP_ESP32:   serie = "";    break;
    default: break;
  }
  snprintf(name, sizeof name, "ESP32%s", serie);
  return name;
}

uint32_t drvChipRevision() { esp_chip_info_t ci; esp_chip_info(&ci); return ci.revision; }
uint32_t drvChipCores()    { esp_chip_info_t ci; esp_chip_info(&ci); return ci.cores; }
const char* drvSdkVersion() { return esp_get_idf_version(); }

void drvBuildDateTo(char* out, size_t n) {
  if (!out || !n) return;
  const esp_app_desc_t* d = esp_app_get_description();
  if (d) snprintf(out, n, "%s %s", d->date, d->time);
  else out[0] = 0;
}

uint32_t drvFlashSize() {
  uint32_t size = 0;
  if (esp_flash_get_physical_size(nullptr, &size) != ESP_OK) return 0;
  return size;
}

// Il sensore si installa alla prima lettura. Intervallo atteso 20..100 C: secondo la documentazione ESP-IDF (driver temperature_sensor,
// ESP32-S3) con questo intervallo l'errore e sotto 2 C (-10..80 C: sotto 1 C; 50..125 C: sotto 3 C). Il chip e sempre piu caldo
// dell'ambiente; oltre 100 C la lettura non e garantita. Le soglie che la usano sono in core/vos_cpupm.cpp.
float drvTempC() {
  static temperature_sensor_handle_t h = nullptr;
  if (!h) {
    temperature_sensor_config_t cfg = TEMPERATURE_SENSOR_CONFIG_DEFAULT(20, 100);
    if (temperature_sensor_install(&cfg, &h) != ESP_OK) { h = nullptr; return NAN; }
    if (temperature_sensor_enable(h) != ESP_OK) { temperature_sensor_uninstall(h); h = nullptr; return NAN; }
  }
  float c = NAN;
  if (temperature_sensor_get_celsius(h, &c) != ESP_OK) return NAN;
  return c;
}
