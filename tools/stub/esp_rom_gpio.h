#pragma once
// STUB di esp_rom_gpio.h (solo le due funzioni che servono al LED del panico)
#include <stdint.h>
void esp_rom_gpio_pad_select_gpio(uint32_t iopad_num);
void esp_rom_gpio_connect_out_signal(uint32_t gpio_num, uint32_t signal_idx, bool out_inv, bool oen_inv);
#define SIG_GPIO_OUT_IDX 128
