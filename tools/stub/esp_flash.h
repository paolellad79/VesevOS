#pragma once
#include "esp_err.h"
#include <stdint.h>
typedef struct esp_flash_t esp_flash_t;
esp_err_t esp_flash_get_physical_size(esp_flash_t*, uint32_t*);
