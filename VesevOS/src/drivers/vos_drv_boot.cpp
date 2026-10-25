// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_drv_boot.cpp (strato 1: avvio della piattaforma e task principale, vedi vos_drv_boot.h)
#include "vos_drv_boot.h"
#include "vos_drv_ota.h"
#include "vos_drv_task.h"
#include <esp_task_wdt.h>
#include <sdkconfig.h>
#include <esp_log.h>
#include <nvs_flash.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

bool drvPlatformInit() {
  drvOtaConfirmBoot();
  esp_log_level_set("*", (esp_log_level_t)CONFIG_LOG_DEFAULT_LEVEL);
  esp_err_t e = nvs_flash_init();
  if (e == ESP_ERR_NVS_NO_FREE_PAGES || e == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    if (nvs_flash_erase() == ESP_OK) e = nvs_flash_init();
  }
  return e == ESP_OK;
}

void drvLogOff(const char* tag) { esp_log_level_set(tag, ESP_LOG_NONE); }

// FreeRTOS scrive l'handle prima di rendere il task eseguibile: chi lo legge dopo lo trova gia valido.
static TaskHandle_t g_mainTask = nullptr;

bool drvMainTaskStart(DrvTaskFn fn, const char* name, uint32_t stackBytes, int priority, int core) {
  return drvTaskCreate(fn, name, stackBytes, nullptr, priority, &g_mainTask, core);
}

bool drvLoopWdtEnable() {
  return g_mainTask && esp_task_wdt_add(g_mainTask) == ESP_OK;
}

void drvLoopWdtFeed() { (void)esp_task_wdt_reset(); }

// Controllo dei task idle e panico: gli stessi della compilazione, cosi la riconfigurazione non cambia nulla all'infuori del tempo.
#ifdef CONFIG_ESP_TASK_WDT_CHECK_IDLE_TASK_CPU0
#define TWDT_IDLE0 1u
#else
#define TWDT_IDLE0 0u
#endif
#ifdef CONFIG_ESP_TASK_WDT_CHECK_IDLE_TASK_CPU1
#define TWDT_IDLE1 2u
#else
#define TWDT_IDLE1 0u
#endif
#ifdef CONFIG_ESP_TASK_WDT_PANIC
#define TWDT_PANIC true
#else
#define TWDT_PANIC false
#endif
#ifndef CONFIG_ESP_TASK_WDT_TIMEOUT_S
#define CONFIG_ESP_TASK_WDT_TIMEOUT_S 30
#endif
#ifndef CONFIG_ESP_INT_WDT_TIMEOUT_MS
#define CONFIG_ESP_INT_WDT_TIMEOUT_MS 1000
#endif

static uint32_t g_twdtSec = CONFIG_ESP_TASK_WDT_TIMEOUT_S;

bool drvTwdtSet(uint32_t sec) {
  if (sec == 0) return false;
  esp_task_wdt_config_t c = {};
  c.timeout_ms = sec * 1000u;
  c.idle_core_mask = TWDT_IDLE0 | TWDT_IDLE1;      // uguale alla compilazione: IDF non iscrive ne toglie nessun task
  c.trigger_panic = TWDT_PANIC;
  if (esp_task_wdt_reconfigure(&c) != ESP_OK) return false;
  g_twdtSec = sec;
  return true;
}

uint32_t drvTwdtSec() { return g_twdtSec; }
uint32_t drvIntWdtMs() { return CONFIG_ESP_INT_WDT_TIMEOUT_MS; }
