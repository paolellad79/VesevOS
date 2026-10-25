// Stub per compila.sh: solo le firme usate da vos_drv_console.cpp (driver USB Serial/JTAG di ESP-IDF 5.5).
#pragma once
#include <stdint.h>
#include <stddef.h>
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
typedef struct { uint32_t tx_buffer_size; uint32_t rx_buffer_size; } usb_serial_jtag_driver_config_t;
esp_err_t usb_serial_jtag_driver_install(usb_serial_jtag_driver_config_t* c);
int usb_serial_jtag_read_bytes(void* buf, uint32_t length, TickType_t ticks_to_wait);
int usb_serial_jtag_write_bytes(const void* src, size_t size, TickType_t ticks_to_wait);
esp_err_t usb_serial_jtag_wait_tx_done(TickType_t ticks_to_wait);
bool usb_serial_jtag_is_connected(void);
bool usb_serial_jtag_is_driver_installed(void);
