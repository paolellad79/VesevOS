#pragma once
#include "esp_err.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
typedef struct { const char* base_path; const char* partition_label; const void* partition; uint8_t format_if_mount_failed:1; uint8_t read_only:1; uint8_t dont_mount:1; uint8_t grow_on_mount:1; } esp_vfs_littlefs_conf_t;
esp_err_t esp_vfs_littlefs_register(const esp_vfs_littlefs_conf_t* conf);
esp_err_t esp_littlefs_format(const char* partition_label);
esp_err_t esp_littlefs_info(const char* partition_label, size_t* total_bytes, size_t* used_bytes);
bool esp_littlefs_mounted(const char* partition_label);
