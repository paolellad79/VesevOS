// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_drv_spi.cpp (vedi vos_drv_spi.h). Bus SPI3 di ESP-IDF, senza DMA di grandi blocchi: i buffer sono nostri, in RAM interna.
#include "vos_drv_spi.h"
#include <string.h>
#include <driver/spi_master.h>
#include <driver/gpio.h>
#include "vos_drv_gpio.h"

static const spi_host_device_t HOST = SPI3_HOST;
static spi_device_handle_t g_dev = nullptr;
static bool g_bus = false;
static int  g_cs = -1, g_mosi = -1, g_miso = -1, g_sck = -1;
static uint8_t g_tx[DRV_SPI_MAX] __attribute__((aligned(4)));
static uint8_t g_rx[DRV_SPI_MAX] __attribute__((aligned(4)));

void drvSpiEnd() {
  if (g_dev) { spi_bus_remove_device(g_dev); g_dev = nullptr; }
  if (g_bus) { spi_bus_free(HOST); g_bus = false; }
  if (g_cs >= 0) { drvGpioMode(g_cs, DG_INPUT); gpio_reset_pin((gpio_num_t)g_cs); }
  if (g_mosi >= 0) gpio_reset_pin((gpio_num_t)g_mosi);
  if (g_miso >= 0) gpio_reset_pin((gpio_num_t)g_miso);
  if (g_sck >= 0) gpio_reset_pin((gpio_num_t)g_sck);
  g_cs = g_mosi = g_miso = g_sck = -1;
}

bool drvSpiBegin(int cs, int mosi, int miso, int sck, uint32_t khz) {
  drvSpiEnd();
  spi_bus_config_t bus = {};
  bus.mosi_io_num = mosi;
  bus.miso_io_num = miso;
  bus.sclk_io_num = sck;
  bus.quadwp_io_num = -1;
  bus.quadhd_io_num = -1;
  bus.max_transfer_sz = DRV_SPI_MAX;
  if (spi_bus_initialize(HOST, &bus, SPI_DMA_CH_AUTO) != ESP_OK) return false;
  g_bus = true;
  spi_device_interface_config_t dc = {};
  dc.clock_speed_hz = (int)(khz * 1000);
  dc.mode = 0;
  dc.spics_io_num = -1;                          // il CS lo muoviamo noi
  dc.queue_size = 1;
  if (spi_bus_add_device(HOST, &dc, &g_dev) != ESP_OK) { g_dev = nullptr; drvSpiEnd(); return false; }
  g_cs = cs; g_mosi = mosi; g_miso = miso; g_sck = sck;
  drvGpioWrite(cs, true);
  drvGpioMode(cs, DG_OUTPUT);
  return true;
}

bool drvSpiTransfer(const uint8_t* tx, uint8_t* rx, size_t n) {
  if (!g_dev || n == 0 || n > DRV_SPI_MAX) return false;
  if (tx) memcpy(g_tx, tx, n); else memset(g_tx, 0, n);
  spi_transaction_t t = {};
  t.length = n * 8;
  t.tx_buffer = g_tx;
  t.rx_buffer = g_rx;
  drvGpioWrite(g_cs, false);
  esp_err_t e = spi_device_polling_transmit(g_dev, &t);
  drvGpioWrite(g_cs, true);
  if (e != ESP_OK) return false;
  if (rx) memcpy(rx, g_rx, n);
  return true;
}
