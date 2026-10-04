#pragma once
#include "esp_err.h"
typedef enum{ESP_SLEEP_WAKEUP_ALL=0} esp_sleep_source_t;
esp_err_t esp_sleep_disable_wakeup_source(esp_sleep_source_t); void esp_deep_sleep_start();
