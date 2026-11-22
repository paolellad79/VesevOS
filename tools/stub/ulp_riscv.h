#pragma once
// Copia ridotta di ulp_riscv.h e ulp_common.h (ESP-IDF 5.5.5): solo cio che usa drivers/vos_drv_ulp.cpp.
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"
esp_err_t ulp_set_wakeup_period(size_t period_index, uint32_t period_us);
esp_err_t ulp_riscv_load_binary(const uint8_t* program_binary, size_t program_size_bytes);
esp_err_t ulp_riscv_run(void);
void ulp_riscv_timer_stop(void);
void ulp_riscv_halt(void);
