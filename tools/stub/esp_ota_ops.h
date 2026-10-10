#pragma once
#include "esp_partition.h"
typedef struct { char project_name[32]; char version[32]; } esp_app_desc_t;
typedef uint32_t esp_ota_handle_t;
#define OTA_SIZE_UNKNOWN 0xffffffffUL
esp_err_t esp_ota_get_partition_description(const esp_partition_t*, esp_app_desc_t*);
esp_err_t esp_ota_set_boot_partition(const esp_partition_t*);
esp_err_t esp_ota_begin(const esp_partition_t*, size_t, esp_ota_handle_t*);
esp_err_t esp_ota_write(esp_ota_handle_t, const void*, size_t);
esp_err_t esp_ota_end(esp_ota_handle_t);
esp_err_t esp_ota_abort(esp_ota_handle_t);
void esp_restart();
