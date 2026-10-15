#pragma once
// STUB ricavato da esp_wps.h (ESP-IDF 5.5)
#include "esp_err.h"
typedef enum { WPS_TYPE_DISABLE = 0, WPS_TYPE_PBC, WPS_TYPE_PIN, WPS_TYPE_MAX } wps_type_t;
typedef struct { wps_type_t wps_type; } esp_wps_config_t;
#define WPS_CONFIG_INIT_DEFAULT(type) { .wps_type = type }
esp_err_t esp_wifi_wps_enable(const esp_wps_config_t* config);
esp_err_t esp_wifi_wps_disable(void);
esp_err_t esp_wifi_wps_start(int timeout_ms);
