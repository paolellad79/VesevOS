#pragma once
// STUB di driver/sdspi_host.h
#include "sdmmc_cmd.h"
#include "driver/gpio.h"
#include "driver/spi_common.h"
typedef int sdspi_dev_handle_t;
typedef struct { spi_host_device_t host_id; gpio_num_t gpio_cs; } sdspi_device_config_t;
#define SDSPI_HOST_DEFAULT() (sdmmc_host_t{0, 0})
#define SDSPI_DEVICE_CONFIG_DEFAULT() (sdspi_device_config_t{SPI2_HOST, (gpio_num_t)0})
esp_err_t sdspi_host_init_device(const sdspi_device_config_t*, sdspi_dev_handle_t*);
esp_err_t sdspi_host_remove_device(sdspi_dev_handle_t);
