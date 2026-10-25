// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_drv_led.cpp
// WS2812: 24 bit per LED in ordine verde, rosso, blu, bit alto per primo. Risoluzione RMT 10 MHz (1 tick = 0,1 us):
// bit 0 = 0,4 us alto + 0,8 us basso; bit 1 = 0,8 us alto + 0,4 us basso (stessi tempi usati finora). Poi la linea resta bassa.
#include "vos_drv_led.h"
#include <driver/rmt_tx.h>
#include <driver/rmt_encoder.h>

static rmt_channel_handle_t g_chan = nullptr;
static rmt_encoder_handle_t g_enc = nullptr;
static int g_pin = -1;

static void closeChannel() {
  if (g_chan) { rmt_disable(g_chan); rmt_del_channel(g_chan); g_chan = nullptr; }
  if (g_enc) { rmt_del_encoder(g_enc); g_enc = nullptr; }
  g_pin = -1;
}

static bool openChannel(int gpio) {
  rmt_tx_channel_config_t cc = {};
  cc.gpio_num = (gpio_num_t)gpio;
  cc.clk_src = RMT_CLK_SRC_DEFAULT;
  cc.resolution_hz = 10000000;
  cc.mem_block_symbols = 48;
  cc.trans_queue_depth = 1;
  if (rmt_new_tx_channel(&cc, &g_chan) != ESP_OK) { g_chan = nullptr; return false; }

  rmt_bytes_encoder_config_t ec = {};
  ec.bit0.level0 = 1; ec.bit0.duration0 = 4; ec.bit0.level1 = 0; ec.bit0.duration1 = 8;
  ec.bit1.level0 = 1; ec.bit1.duration0 = 8; ec.bit1.level1 = 0; ec.bit1.duration1 = 4;
  ec.flags.msb_first = 1;
  if (rmt_new_bytes_encoder(&ec, &g_enc) != ESP_OK) { g_enc = nullptr; closeChannel(); return false; }
  if (rmt_enable(g_chan) != ESP_OK) { closeChannel(); return false; }
  g_pin = gpio;
  return true;
}

void drvLedWrite(int gpio, uint8_t r, uint8_t g, uint8_t b) {
  if (g_chan && g_pin != gpio) closeChannel();
  if (!g_chan && !openChannel(gpio)) return;
  uint8_t grb[3] = { g, r, b };
  rmt_transmit_config_t tc = {};
  tc.loop_count = 0;
  tc.flags.eot_level = 0;
  if (rmt_transmit(g_chan, g_enc, grb, sizeof grb, &tc) == ESP_OK) rmt_tx_wait_all_done(g_chan, 50);
}
