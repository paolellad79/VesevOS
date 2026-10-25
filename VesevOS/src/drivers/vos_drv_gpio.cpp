// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_drv_gpio.cpp
// GPIO digitali con driver/gpio.h e ADC con adc_oneshot (come il core Arduino: 12 bit, attenuazione 12 dB).
// Va chiamato da un solo task alla volta (il ciclo principale), come prima.
#include "vos_drv_gpio.h"
#include <driver/gpio.h>
#include <esp_adc/adc_oneshot.h>
#include <esp_adc/adc_cali.h>
#include <esp_adc/adc_cali_scheme.h>
#include <soc/soc_caps.h>

// ---- ADC: stato per unita e per pin ----
static const adc_atten_t ADC_ATTEN = ADC_ATTEN_DB_12;
static const adc_bitwidth_t ADC_BITS = ADC_BITWIDTH_12;
static adc_oneshot_unit_handle_t g_unit[SOC_ADC_PERIPH_NUM];
static adc_cali_handle_t g_cali[GPIO_NUM_MAX];
static uint64_t g_chanReady = 0;          // bit n = il pin n e gia configurato come ingresso analogico

// Prepara unita e canale del pin. false se il pin non e un ingresso analogico.
static bool adcPrepare(int gpio, adc_unit_t& unit, adc_channel_t& ch) {
  if (gpio < 0 || gpio >= GPIO_NUM_MAX) return false;
  if (adc_oneshot_io_to_channel(gpio, &unit, &ch) != ESP_OK) return false;
  if (!g_unit[unit]) {
    adc_oneshot_unit_init_cfg_t ic = {};
    ic.unit_id = unit;
    ic.ulp_mode = ADC_ULP_MODE_DISABLE;
    if (adc_oneshot_new_unit(&ic, &g_unit[unit]) != ESP_OK) { g_unit[unit] = nullptr; return false; }
  }
  if (!(g_chanReady & (1ULL << gpio))) {
    adc_oneshot_chan_cfg_t cc = {};
    cc.atten = ADC_ATTEN;
    cc.bitwidth = ADC_BITS;
    if (adc_oneshot_config_channel(g_unit[unit], ch, &cc) != ESP_OK) return false;
    g_chanReady |= 1ULL << gpio;
  }
  return true;
}

// Curva di calibrazione di fabbrica (valori scritti negli eFuse). nullptr se il chip non ne ha.
static adc_cali_handle_t adcCali(int gpio, adc_unit_t unit, adc_channel_t ch) {
  if (g_cali[gpio]) return g_cali[gpio];
  adc_cali_handle_t h = nullptr;
#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
  adc_cali_curve_fitting_config_t cc = {};
  cc.unit_id = unit; cc.chan = ch; cc.atten = ADC_ATTEN; cc.bitwidth = ADC_BITS;
  if (adc_cali_create_scheme_curve_fitting(&cc, &h) != ESP_OK) h = nullptr;
#elif ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
  adc_cali_line_fitting_config_t cc = {};
  cc.unit_id = unit; cc.atten = ADC_ATTEN; cc.bitwidth = ADC_BITS;
  if (adc_cali_create_scheme_line_fitting(&cc, &h) != ESP_OK) h = nullptr;
#endif
  g_cali[gpio] = h;
  return h;
}

int drvGpioAnalog(int gpio) {
  adc_unit_t unit; adc_channel_t ch;
  if (!adcPrepare(gpio, unit, ch)) return 0;
  int raw = 0;
  if (adc_oneshot_read(g_unit[unit], ch, &raw) != ESP_OK) return 0;
  return raw;
}

int drvGpioAnalogMv(int gpio) {
  adc_unit_t unit; adc_channel_t ch;
  if (!adcPrepare(gpio, unit, ch)) return -1;
  adc_cali_handle_t cali = adcCali(gpio, unit, ch);
  if (!cali) return -1;
  int raw = 0, mv = 0;
  if (adc_oneshot_read(g_unit[unit], ch, &raw) != ESP_OK) return -1;
  if (adc_cali_raw_to_voltage(cali, raw, &mv) != ESP_OK) return -1;
  return mv;
}

// ---- digitali ----
void drvGpioMode(int gpio, DrvGpioMode m) {
  if (gpio < 0 || gpio >= GPIO_NUM_MAX) return;
  gpio_config_t c = {};
  c.pin_bit_mask = 1ULL << gpio;
  c.mode = m == DG_OUTPUT ? GPIO_MODE_INPUT_OUTPUT : GPIO_MODE_INPUT;
  c.pull_up_en = m == DG_INPUT_PULLUP ? GPIO_PULLUP_ENABLE : GPIO_PULLUP_DISABLE;
  c.pull_down_en = m == DG_INPUT_PULLDOWN ? GPIO_PULLDOWN_ENABLE : GPIO_PULLDOWN_DISABLE;
  c.intr_type = GPIO_INTR_DISABLE;
  gpio_config(&c);
  g_chanReady &= ~(1ULL << gpio);         // il pin torna digitale: la prossima lettura analogica lo riconfigura
}
void drvGpioWrite(int gpio, bool high) { gpio_set_level((gpio_num_t)gpio, high ? 1 : 0); }
bool drvGpioRead(int gpio) { return gpio_get_level((gpio_num_t)gpio) != 0; }
