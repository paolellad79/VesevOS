#pragma once
#include "esp_err.h"
#include <stdint.h>
typedef enum{ESP_SLEEP_WAKEUP_ALL=0,ESP_SLEEP_WAKEUP_TIMER=4} esp_sleep_source_t;
esp_err_t esp_sleep_disable_wakeup_source(esp_sleep_source_t); void esp_deep_sleep_start();
esp_err_t esp_sleep_enable_timer_wakeup(uint64_t time_in_us); esp_sleep_source_t esp_sleep_get_wakeup_cause();
