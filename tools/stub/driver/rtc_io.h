#pragma once
#include "gpio.h"
esp_err_t rtc_gpio_pullup_en(gpio_num_t); esp_err_t rtc_gpio_pullup_dis(gpio_num_t); esp_err_t rtc_gpio_pulldown_en(gpio_num_t); esp_err_t rtc_gpio_pulldown_dis(gpio_num_t);
// Funzioni usate da drivers/vos_drv_ulp.cpp (stesse firme di driver/rtc_io.h dell'IDF 5.5.5)
typedef enum { RTC_GPIO_MODE_INPUT_ONLY, RTC_GPIO_MODE_OUTPUT_ONLY, RTC_GPIO_MODE_INPUT_OUTPUT, RTC_GPIO_MODE_DISABLED, RTC_GPIO_MODE_OUTPUT_OD, RTC_GPIO_MODE_INPUT_OUTPUT_OD } rtc_gpio_mode_t;
bool rtc_gpio_is_valid_gpio(gpio_num_t); esp_err_t rtc_gpio_init(gpio_num_t); esp_err_t rtc_gpio_deinit(gpio_num_t);
esp_err_t rtc_gpio_set_direction(gpio_num_t, rtc_gpio_mode_t); esp_err_t rtc_gpio_hold_en(gpio_num_t); esp_err_t rtc_gpio_hold_dis(gpio_num_t);
