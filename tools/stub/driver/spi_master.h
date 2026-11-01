#pragma once
// STUB di driver/spi_master.h: solo cio che usa drivers/vos_drv_spi.cpp
#include "spi_common.h"
typedef struct spi_device_t* spi_device_handle_t;
typedef struct { int mode; int clock_speed_hz; int spics_io_num; int queue_size; } spi_device_interface_config_t;
typedef struct { size_t length; const void* tx_buffer; void* rx_buffer; } spi_transaction_t;
esp_err_t spi_bus_add_device(spi_host_device_t, const spi_device_interface_config_t*, spi_device_handle_t*);
esp_err_t spi_bus_remove_device(spi_device_handle_t);
esp_err_t spi_device_polling_transmit(spi_device_handle_t, spi_transaction_t*);
