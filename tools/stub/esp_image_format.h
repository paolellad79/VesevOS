#pragma once
#include "esp_partition.h"
typedef struct { uint32_t offset; uint32_t size; } esp_partition_pos_t;
typedef struct { uint32_t start_addr; uint32_t image_len; } esp_image_metadata_t;
typedef enum { ESP_IMAGE_VERIFY, ESP_IMAGE_LOAD } esp_image_load_mode_t;
esp_err_t esp_image_verify(esp_image_load_mode_t, const esp_partition_pos_t*, esp_image_metadata_t*);
