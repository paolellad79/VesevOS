#pragma once
// STUB di driver/spi_common.h
#include "esp_err.h"
typedef enum { SPI1_HOST = 0, SPI2_HOST = 1, SPI3_HOST = 2 } spi_host_device_t;
#define SPI_DMA_CH_AUTO 3
typedef struct { int mosi_io_num, miso_io_num, sclk_io_num, quadwp_io_num, quadhd_io_num, max_transfer_sz; } spi_bus_config_t;
esp_err_t spi_bus_initialize(spi_host_device_t, const spi_bus_config_t*, int);
esp_err_t spi_bus_free(spi_host_device_t);
