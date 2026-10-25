// VesevOS Recovery - LED di stato (vedi rec_led.h). Segnale WS2812 con il periferico RMT (stessa temporizzazione di VesevOS):
// bit 0 = 0,4 us alto + 0,8 us basso; bit 1 = 0,8 us alto + 0,4 us basso; ordine dei colori verde, rosso, blu.
#include "rec_led.h"
#include "rec_util.h"
#include <atomic>
#include <driver/rmt_tx.h>
#include <driver/rmt_encoder.h>

static rmt_channel_handle_t g_chan = nullptr;
static rmt_encoder_handle_t g_enc = nullptr;
static int g_pin = -1, g_bright = 80;
static bool g_on = false;
static std::atomic<int> g_state{LS_WAIT};
static std::atomic<uint32_t> g_errTime{0};
static uint32_t g_lastKey = 0xFFFFFFFFUL;

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
  if (rmt_new_bytes_encoder(&ec, &g_enc) != ESP_OK) { g_enc = nullptr; return false; }
  return rmt_enable(g_chan) == ESP_OK;
}

static void writePixel(uint8_t r, uint8_t g, uint8_t b) {
  if (!g_chan && !openChannel(g_pin)) return;
  uint8_t grb[3] = { g, r, b };
  rmt_transmit_config_t tc = {};
  tc.flags.eot_level = 0;
  if (rmt_transmit(g_chan, g_enc, grb, sizeof grb, &tc) == ESP_OK) rmt_tx_wait_all_done(g_chan, 50);
}

void recLedStart(int pin, int brightness) {
  g_pin = pin;
  g_bright = brightness;
  g_on = true;
}

void recLedSet(int state) {
  if (state == LS_ERR) g_errTime = recMillis();
  g_state = state;
}

void recLedTick() {
  if (!g_on) return;
  uint32_t n = recMillis();
  if (g_state == LS_ERR && n - g_errTime > 10000UL) g_state = LS_WAIT;
  uint8_t r = 0, g = 0, b = 0;
  switch (g_state) {
    case LS_WAIT:  if ((n % 1600UL) < 800UL) { r = 255; g = 120; } break;       // arancione lento
    case LS_WRITE: if ((n / 150UL) & 1) { g = 160; b = 255; } break;            // azzurro veloce
    case LS_DONE:  g = 255; break;                                               // verde fisso
    default:       r = 255; break;                                               // rosso fisso
  }
  r = (uint8_t)((uint16_t)r * g_bright / 255);
  g = (uint8_t)((uint16_t)g * g_bright / 255);
  b = (uint8_t)((uint16_t)b * g_bright / 255);
  uint32_t key = ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
  if (key == g_lastKey) return;
  g_lastKey = key;
  writePixel(r, g, b);
}
