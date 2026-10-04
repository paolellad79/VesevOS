#pragma once
#include "freertos/FreeRTOS.h"
#include "esp_err.h"
typedef bool (*esp_freertos_idle_cb_t)(); esp_err_t esp_register_freertos_idle_hook_for_cpu(esp_freertos_idle_cb_t,int);
