#pragma once
#include <stdint.h>
#include <stddef.h>
typedef int esp_err_t;
#ifndef ESP_OK
#define ESP_OK 0
#endif
typedef enum { ESP_PARTITION_TYPE_APP = 0, ESP_PARTITION_TYPE_DATA = 1 } esp_partition_type_t;
typedef enum { ESP_PARTITION_SUBTYPE_APP_FACTORY = 0, ESP_PARTITION_SUBTYPE_APP_OTA_0 = 0x10, ESP_PARTITION_SUBTYPE_DATA_OTA = 0x00 } esp_partition_subtype_t;
typedef struct { uint32_t address; uint32_t size; } esp_partition_t;
const esp_partition_t* esp_partition_find_first(esp_partition_type_t, esp_partition_subtype_t, const char*);
esp_err_t esp_partition_read(const esp_partition_t*, size_t, void*, size_t);
esp_err_t esp_partition_erase_range(const esp_partition_t*, size_t, size_t);
