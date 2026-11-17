// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_drv_led.cpp
// WS2812: 24 bit per LED in ordine verde, rosso, blu (o quello di VOS_LED_ORDER nel file di scheda), bit alto per primo. Risoluzione RMT 10 MHz (1 tick = 0,1 us):
// bit 0 = 0,4 us alto + 0,8 us basso; bit 1 = 0,8 us alto + 0,4 us basso (stessi tempi usati finora). Poi la linea resta bassa.
// Il LED di bordo e i LED a mano usano lo stesso codice (apriCanale, inviaColore): cambia solo chi tiene i canali.
#include "vos_drv_led.h"
#include <driver/rmt_tx.h>
#include <driver/rmt_encoder.h>
#include <soc/soc_caps.h>
#include "vos_drv_time.h"
#include "../boards/vos_board.h"

struct Canale {
  rmt_channel_handle_t chan;
  rmt_encoder_handle_t enc;
  int pin;
};

static Canale g_bordo = { nullptr, nullptr, -1 };      // il LED di bordo (un solo pin, cambiabile)

// Protezione anti-ciclo del LED di bordo: se il canale non si apre (pin che non esiste, periferica assente) non si riprova ogni 40 ms.
// Dopo il primo errore: nuovi tentativi dopo 1 s, 10 s, 60 s; poi si smette finche il pin non cambia.
static const uint32_t RETRY_MS[] = { 1000, 10000, 60000 };
static const uint8_t MAX_FAILS = 4;                    // primo tentativo + 3 ripetizioni
static uint8_t g_fails = 0;
static int g_failPin = -1;
static uint32_t g_nextTry = 0;

static void chiudiCanale(Canale& c) {
  if (c.chan) { rmt_disable(c.chan); rmt_del_channel(c.chan); c.chan = nullptr; }
  if (c.enc) { rmt_del_encoder(c.enc); c.enc = nullptr; }
  c.pin = -1;
}

static bool apriCanale(Canale& c, int gpio) {
  rmt_tx_channel_config_t cc = {};
  cc.gpio_num = (gpio_num_t)gpio;
  cc.clk_src = RMT_CLK_SRC_DEFAULT;                  // S3: APB 80 MHz; C6: PLL_F80M 80 MHz. In tutti e due 80/10 = 8 esatto: tempi uguali
  cc.resolution_hz = 10000000;
  cc.mem_block_symbols = 48;
  cc.trans_queue_depth = 1;
  if (rmt_new_tx_channel(&cc, &c.chan) != ESP_OK) { c.chan = nullptr; return false; }

  rmt_bytes_encoder_config_t ec = {};
  ec.bit0.level0 = 1; ec.bit0.duration0 = 4; ec.bit0.level1 = 0; ec.bit0.duration1 = 8;
  ec.bit1.level0 = 1; ec.bit1.duration0 = 8; ec.bit1.level1 = 0; ec.bit1.duration1 = 4;
  ec.flags.msb_first = 1;
  if (rmt_new_bytes_encoder(&ec, &c.enc) != ESP_OK) { c.enc = nullptr; chiudiCanale(c); return false; }
  if (rmt_enable(c.chan) != ESP_OK) { chiudiCanale(c); return false; }
  c.pin = gpio;
  return true;
}

// Mette i tre colori nell'ordine dei byte del LED (VOS_LED_ORD_*: GRB e il WS2812 standard, valore anche per un numero sconosciuto).
static void orderBytes(uint8_t ordine, uint8_t* out, uint8_t r, uint8_t g, uint8_t b) {
  switch (ordine) {
    case VOS_LED_ORD_RGB: out[0] = r; out[1] = g; out[2] = b; break;
    case VOS_LED_ORD_BRG: out[0] = b; out[1] = r; out[2] = g; break;
    case VOS_LED_ORD_GBR: out[0] = g; out[1] = b; out[2] = r; break;
    case VOS_LED_ORD_RBG: out[0] = r; out[1] = b; out[2] = g; break;
    case VOS_LED_ORD_BGR: out[0] = b; out[1] = g; out[2] = r; break;
    default:              out[0] = g; out[1] = r; out[2] = b; break;
  }
}

static void inviaColore(Canale& c, uint8_t ordine, uint8_t r, uint8_t g, uint8_t b) {
  uint8_t bytes[3];
  orderBytes(ordine, bytes, r, g, b);
  rmt_transmit_config_t tc = {};
  tc.loop_count = 0;                                 // una sola trasmissione: l'errata RMT-176 riguarda solo la modalita continua
  tc.flags.eot_level = 0;
  if (rmt_transmit(c.chan, c.enc, bytes, sizeof bytes, &tc) == ESP_OK) rmt_tx_wait_all_done(c.chan, 50);
}

// ---------------- LED di bordo ----------------
bool drvLedAbsent() { return g_fails >= MAX_FAILS; }

void drvLedWrite(int gpio, uint8_t r, uint8_t g, uint8_t b) {
  if (g_bordo.chan && g_bordo.pin != gpio) chiudiCanale(g_bordo);
  if (gpio != g_failPin) { g_failPin = gpio; g_fails = 0; }    // pin nuovo: si riparte da zero
  if (!g_bordo.chan) {
    if (g_fails >= MAX_FAILS) return;                          // assente: ritorno subito, senza errori
    if (g_fails && (int32_t)(drvMillis() - g_nextTry) < 0) return;   // attesa lunga tra i tentativi
    if (!apriCanale(g_bordo, gpio)) {
      g_nextTry = drvMillis() + RETRY_MS[g_fails < 3 ? g_fails : 2];
      g_fails++;
      return;
    }
    g_fails = 0;
  }
  inviaColore(g_bordo, VOS_LED_ORDER, r, g, b);
}

// ---------------- LED a mano ----------------
// Canali di trasmissione del chip (S3: 4, C6/C3/H2: 2) meno quello del LED di bordo, se la scheda lo ha. Datasheet dei chip, capitolo RMT.
#define LED_CANALI SOC_RMT_TX_CANDIDATES_PER_GROUP
struct Mano { Canale c; bool usato; uint8_t ordine; };
static Mano g_mano[LED_CANALI];

int drvLedLiberi() {
  int n = LED_CANALI - (VOS_HAS_LED_RGB ? 1 : 0);
  for (int i = 0; i < LED_CANALI; i++) if (g_mano[i].usato) n--;
  return n > 0 ? n : 0;
}

int drvLedApri(int gpio, uint8_t ordine) {
  if (drvLedLiberi() < 1) return DRV_LED_E_CANALI;
  for (int i = 0; i < LED_CANALI; i++) {
    if (g_mano[i].usato) continue;
    if (!apriCanale(g_mano[i].c, gpio)) return DRV_LED_E_HW;
    g_mano[i].usato = true;
    g_mano[i].ordine = ordine > DRV_LED_ORD_MAX ? 0 : ordine;
    return i;
  }
  return DRV_LED_E_CANALI;
}

void drvLedScrivi(int h, uint8_t r, uint8_t g, uint8_t b) {
  if (h < 0 || h >= LED_CANALI || !g_mano[h].usato) return;
  inviaColore(g_mano[h].c, g_mano[h].ordine, r, g, b);
}

void drvLedOrdine(int h, uint8_t ordine) {
  if (h < 0 || h >= LED_CANALI || !g_mano[h].usato) return;
  g_mano[h].ordine = ordine > DRV_LED_ORD_MAX ? 0 : ordine;
}

void drvLedChiudi(int h) {
  if (h < 0 || h >= LED_CANALI || !g_mano[h].usato) return;
  chiudiCanale(g_mano[h].c);
  g_mano[h].usato = false;
}
