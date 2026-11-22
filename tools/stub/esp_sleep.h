#pragma once
#include "esp_err.h"
#include <stdint.h>
// Copia ridotta dell'elenco di esp_sleep.h (ESP-IDF 5.5.5): stessi nomi e stesso ordine.
typedef enum{ESP_SLEEP_WAKEUP_UNDEFINED=0,ESP_SLEEP_WAKEUP_ALL,ESP_SLEEP_WAKEUP_EXT0,ESP_SLEEP_WAKEUP_EXT1,ESP_SLEEP_WAKEUP_TIMER,
  ESP_SLEEP_WAKEUP_TOUCHPAD,ESP_SLEEP_WAKEUP_ULP,ESP_SLEEP_WAKEUP_GPIO,ESP_SLEEP_WAKEUP_UART,ESP_SLEEP_WAKEUP_UART1,ESP_SLEEP_WAKEUP_UART2,
  ESP_SLEEP_WAKEUP_WIFI,ESP_SLEEP_WAKEUP_COCPU,ESP_SLEEP_WAKEUP_COCPU_TRAP_TRIG} esp_sleep_source_t;
typedef esp_sleep_source_t esp_sleep_wakeup_cause_t;
esp_err_t esp_sleep_disable_wakeup_source(esp_sleep_source_t); void esp_deep_sleep_start();
esp_err_t esp_sleep_enable_timer_wakeup(uint64_t time_in_us); esp_sleep_source_t esp_sleep_get_wakeup_cause();
esp_err_t esp_light_sleep_start(); esp_err_t esp_sleep_enable_gpio_wakeup();
typedef enum{ESP_EXT1_WAKEUP_ANY_LOW=0,ESP_EXT1_WAKEUP_ANY_HIGH=1} esp_sleep_ext1_wakeup_mode_t;
esp_err_t esp_sleep_enable_ext1_wakeup_io(uint64_t io_mask, esp_sleep_ext1_wakeup_mode_t level_mode);
typedef enum{ESP_GPIO_WAKEUP_GPIO_LOW=0,ESP_GPIO_WAKEUP_GPIO_HIGH=1} esp_deepsleep_gpio_wake_up_mode_t;
esp_err_t esp_deep_sleep_enable_gpio_wakeup(uint64_t gpio_pin_mask, esp_deepsleep_gpio_wake_up_mode_t mode);
enum { ESP_ERR_SLEEP_REJECT = ESP_ERR_INVALID_STATE, ESP_ERR_SLEEP_TOO_SHORT_SLEEP_DURATION = 0x102 };   // come in esp_sleep.h dell'IDF (la seconda vale ESP_ERR_INVALID_ARG)
// Per il coprocessore ULP (vos_drv_ulp.cpp)
typedef enum { ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_DOMAIN_MAX } esp_sleep_pd_domain_t;
typedef enum { ESP_PD_OPTION_OFF, ESP_PD_OPTION_ON, ESP_PD_OPTION_AUTO } esp_sleep_pd_option_t;
esp_err_t esp_sleep_pd_config(esp_sleep_pd_domain_t, esp_sleep_pd_option_t); esp_err_t esp_sleep_enable_ulp_wakeup();
