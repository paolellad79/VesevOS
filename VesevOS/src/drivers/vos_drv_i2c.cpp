// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_drv_i2c.cpp
// Bus I2C master di ESP-IDF (driver/i2c_master.h). Un dispositivo per indirizzo, creato alla prima richiesta.
#include "vos_drv_i2c.h"
#include <string.h>
#include <driver/i2c_master.h>
#include <driver/gpio.h>

static const int      MAX_DEVS = 4;           // sul bus ci sono pochi sensori
static const int      TIMEOUT_MS = 20;
static i2c_master_bus_handle_t g_bus = nullptr;
static uint32_t       g_hz = 100000;
static int            g_sda = -1, g_scl = -1;
static struct { uint8_t addr; i2c_master_dev_handle_t h; } g_dev[MAX_DEVS];
static int            g_ndev = 0;

static void releaseBus() {
  for (int i = 0; i < g_ndev; i++) i2c_master_bus_rm_device(g_dev[i].h);
  g_ndev = 0;
  if (g_bus) { i2c_del_master_bus(g_bus); g_bus = nullptr; }
  if (g_sda >= 0) gpio_reset_pin((gpio_num_t)g_sda);
  if (g_scl >= 0) gpio_reset_pin((gpio_num_t)g_scl);
  g_sda = g_scl = -1;
}

bool drvI2cBegin(int sda, int scl, uint32_t hz) {
  releaseBus();
  i2c_master_bus_config_t bc = {};
  bc.i2c_port = -1;                              // porta scelta dal driver
  bc.sda_io_num = (gpio_num_t)sda;
  bc.scl_io_num = (gpio_num_t)scl;
  bc.clk_source = I2C_CLK_SRC_DEFAULT;
  bc.glitch_ignore_cnt = 7;
  bc.flags.enable_internal_pullup = true;        // come Wire: le resistenze interne restano accese
  if (i2c_new_master_bus(&bc, &g_bus) != ESP_OK) { g_bus = nullptr; return false; }
  g_hz = hz; g_sda = sda; g_scl = scl;
  return true;
}

void drvI2cEnd() { releaseBus(); }

// Dispositivo per l'indirizzo (lo crea la prima volta). nullptr se non ce n'e piu o il bus non va.
static i2c_master_dev_handle_t devFor(uint8_t addr) {
  for (int i = 0; i < g_ndev; i++) if (g_dev[i].addr == addr) return g_dev[i].h;
  if (g_ndev >= MAX_DEVS) return nullptr;
  i2c_device_config_t dc = {};
  dc.dev_addr_length = I2C_ADDR_BIT_LEN_7;
  dc.device_address = addr;
  dc.scl_speed_hz = g_hz;
  i2c_master_dev_handle_t h = nullptr;
  if (i2c_master_bus_add_device(g_bus, &dc, &h) != ESP_OK) return nullptr;
  g_dev[g_ndev].addr = addr; g_dev[g_ndev].h = h; g_ndev++;
  return h;
}

// Il driver non distingue "indirizzo non risponde" da "dato rifiutato": NACK generico
static uint8_t errCode(esp_err_t e) {
  if (e == ESP_OK) return DI_OK;
  if (e == ESP_ERR_TIMEOUT) return DI_TIMEOUT;
  if (e == ESP_FAIL) return DI_NACK;
  return DI_OTHER;
}

uint8_t drvI2cReadReg(uint8_t addr, uint8_t reg, uint8_t* buf, size_t n) {
  if (!g_bus) return DI_NOBUS;
  if (!n || n > 32) return DI_OTHER;
  i2c_master_dev_handle_t d = devFor(addr);
  if (!d) return DI_OTHER;
  return errCode(i2c_master_transmit_receive(d, &reg, 1, buf, n, TIMEOUT_MS));
}

uint8_t drvI2cWriteReg(uint8_t addr, uint8_t reg, const uint8_t* buf, size_t n) {
  if (!g_bus) return DI_NOBUS;
  if (n > 30) return DI_OTHER;
  i2c_master_dev_handle_t d = devFor(addr);
  if (!d) return DI_OTHER;
  uint8_t out[31];
  out[0] = reg;
  if (n) memcpy(out + 1, buf, n);
  return errCode(i2c_master_transmit(d, out, n + 1, TIMEOUT_MS));
}
