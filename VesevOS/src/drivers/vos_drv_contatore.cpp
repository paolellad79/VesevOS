// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_drv_contatore.cpp
// Contatore di impulsi con PCNT (vedi vos_drv_contatore.h): un canale per unita, solo il pin degli impulsi.
#include "vos_drv_contatore.h"
#include "vos_drv_gpio.h"
#include <soc/soc_caps.h>

#if SOC_PCNT_SUPPORTED
#include <driver/pulse_cnt.h>

struct Unita { pcnt_unit_handle_t unit; pcnt_channel_handle_t canale; };

static Unita g_u[SOC_PCNT_UNITS_PER_GROUP];
static bool  g_usata[SOC_PCNT_UNITS_PER_GROUP];

static void libera(int h) {
  if (g_u[h].canale) pcnt_del_channel(g_u[h].canale);
  if (g_u[h].unit) { pcnt_unit_stop(g_u[h].unit); pcnt_unit_disable(g_u[h].unit); pcnt_del_unit(g_u[h].unit); }
  g_u[h] = Unita{};
  g_usata[h] = false;
}

// Il pin lo configura pcnt_new_channel (con la resistenza verso l'alto): qui si porta al valore scelto.
static void tiraPin(int gpio, uint8_t pull) {
  drvGpioResistenze(gpio, pull == DRV_CONT_PULL_SU, pull == DRV_CONT_PULL_GIU);
}

int drvContatoreApri(int gpio, uint8_t fronte, uint16_t filtroNs, uint8_t pull) {
  if (filtroNs > DRV_CONT_FILTRO_MAX) return DRV_CONT_E_FILTRO;
  int h = -1;
  for (int i = 0; i < SOC_PCNT_UNITS_PER_GROUP && h < 0; i++) if (!g_usata[i]) h = i;
  if (h < 0) return DRV_CONT_E_UNITA;
  g_usata[h] = true;
  g_u[h] = Unita{};
  pcnt_unit_config_t uc = {};
  uc.low_limit = -32768;
  uc.high_limit = 32767;
  uc.flags.accum_count = 1;
  if (pcnt_new_unit(&uc, &g_u[h].unit) != ESP_OK) { g_u[h].unit = nullptr; libera(h); return DRV_CONT_E_HW; }
  if (filtroNs) {
    pcnt_glitch_filter_config_t fc = {};
    fc.max_glitch_ns = filtroNs;
    if (pcnt_unit_set_glitch_filter(g_u[h].unit, &fc) != ESP_OK) { libera(h); return DRV_CONT_E_FILTRO; }
  }
  pcnt_chan_config_t cc = {};
  cc.edge_gpio_num = gpio;
  cc.level_gpio_num = -1;
  if (pcnt_new_channel(g_u[h].unit, &cc, &g_u[h].canale) != ESP_OK) { g_u[h].canale = nullptr; libera(h); return DRV_CONT_E_HW; }
  pcnt_channel_edge_action_t su = fronte == DRV_CONT_FRONTE_DISCESA ? PCNT_CHANNEL_EDGE_ACTION_HOLD : PCNT_CHANNEL_EDGE_ACTION_INCREASE;
  pcnt_channel_edge_action_t giu = fronte == DRV_CONT_FRONTE_SALITA ? PCNT_CHANNEL_EDGE_ACTION_HOLD : PCNT_CHANNEL_EDGE_ACTION_INCREASE;
  tiraPin(gpio, pull);
  if (pcnt_channel_set_edge_action(g_u[h].canale, su, giu) != ESP_OK || pcnt_unit_enable(g_u[h].unit) != ESP_OK) { libera(h); return DRV_CONT_E_HW; }
  pcnt_unit_clear_count(g_u[h].unit);
  if (pcnt_unit_start(g_u[h].unit) != ESP_OK) { libera(h); return DRV_CONT_E_HW; }
  return h;
}

uint32_t drvContatoreLeggi(int h) {
  int v = 0;
  if (h >= 0 && h < SOC_PCNT_UNITS_PER_GROUP && g_usata[h]) pcnt_unit_get_count(g_u[h].unit, &v);
  return (uint32_t)v;
}

void drvContatoreAzzera(int h) {
  if (h >= 0 && h < SOC_PCNT_UNITS_PER_GROUP && g_usata[h]) pcnt_unit_clear_count(g_u[h].unit);
}

void drvContatoreChiudi(int h) {
  if (h >= 0 && h < SOC_PCNT_UNITS_PER_GROUP && g_usata[h]) libera(h);
}

int drvContatoreTotali() { return SOC_PCNT_UNITS_PER_GROUP; }

int drvContatoreLiberi() {
  int n = 0;
  for (int i = 0; i < SOC_PCNT_UNITS_PER_GROUP; i++) if (!g_usata[i]) n++;
  return n;
}
#endif
