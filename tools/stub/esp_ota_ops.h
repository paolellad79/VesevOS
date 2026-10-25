#pragma once
#include "esp_partition.h"
#include "esp_app_desc.h"
typedef uint32_t esp_ota_handle_t;
#define OTA_SIZE_UNKNOWN 0xffffffffUL
esp_err_t esp_ota_get_partition_description(const esp_partition_t*, esp_app_desc_t*);
esp_err_t esp_ota_set_boot_partition(const esp_partition_t*);
esp_err_t esp_ota_begin(const esp_partition_t*, size_t, esp_ota_handle_t*);
esp_err_t esp_ota_write(esp_ota_handle_t, const void*, size_t);
esp_err_t esp_ota_end(esp_ota_handle_t);
esp_err_t esp_ota_abort(esp_ota_handle_t);
void esp_restart();
typedef enum { ESP_OTA_IMG_NEW = 0, ESP_OTA_IMG_PENDING_VERIFY = 1, ESP_OTA_IMG_VALID = 2, ESP_OTA_IMG_INVALID = 3, ESP_OTA_IMG_ABORTED = 4, ESP_OTA_IMG_UNDEFINED = 0xFFFFFFFF } esp_ota_img_states_t;
const esp_partition_t* esp_ota_get_running_partition();
const esp_partition_t* esp_ota_get_boot_partition();
const esp_partition_t* esp_ota_get_last_invalid_partition();
esp_err_t esp_ota_get_state_partition(const esp_partition_t*, esp_ota_img_states_t*);
const esp_partition_t* esp_ota_get_next_update_partition(const esp_partition_t*);
esp_err_t esp_ota_mark_app_valid_cancel_rollback(void);
