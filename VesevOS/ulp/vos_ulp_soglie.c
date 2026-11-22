// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - ulp/vos_ulp_soglie.c
// Programma FISSO del coprocessore ULP-RISC-V (solo S3): a ogni giro del timer scorre la tabella di soglie, legge le sorgenti attive e,
// se una riga con azione SVEGLIA diventa vera, sveglia la CPU principale. Non interpreta niente e non scrive registri RTC a mano:
// usa solo le funzioni di ESP-IDF (ulp_riscv_gpio_get_level, ulp_riscv_adc_read_channel, ulp_riscv_wakeup_main_processor).
// Girano sul coprocessore: niente printf, niente heap. Compilato con la toolchain RISC-V di ESP-IDF (ulp_embed_binary).
#include <stdint.h>
#include "ulp_riscv_utils.h"
#include "ulp_riscv_gpio.h"
#include "ulp_riscv_adc_ulp_core.h"
#include "vos_ulp_tabella.h"

volatile vos_ulp_tabella_t tabella;          // la CPU la vede come ulp_tabella (intestazione generata ulp_vos_soglie.h)

static uint32_t condizione_vera(const volatile vos_ulp_riga_t* r, uint32_t v, uint32_t gia_vera) {
  uint32_t is = r->isteresi;
  switch (r->condizione) {
    case ULP_COND_ALTO:   return v != 0;
    case ULP_COND_BASSO:  return v == 0;
    case ULP_COND_SOPRA:  return gia_vera ? (v + is > r->soglia1) : (v > r->soglia1);                  // gia vera: scende sotto solo oltre l'isteresi
    case ULP_COND_SOTTO:  return gia_vera ? (v < r->soglia1 + is) : (v < r->soglia1);
    case ULP_COND_FUORI:  return v < r->soglia1 || v > r->soglia2;
    case ULP_COND_DENTRO: return v >= r->soglia1 && v <= r->soglia2;
    default:              return 0;
  }
}

int main(void) {
  uint32_t sveglia = 0;
  tabella.cicli++;
  for (int i = 0; i < ULP_RIGHE_MAX; i++) {
    volatile vos_ulp_riga_t* r = &tabella.righe[i];
    volatile vos_ulp_stato_t* s = &tabella.stato[i];
    if (r->tipo == ULP_TIPO_NESSUNO) continue;
    uint32_t v;
    if (r->tipo == ULP_TIPO_LIVELLO) v = ulp_riscv_gpio_get_level((gpio_num_t)r->canale);
    else if (r->tipo == ULP_TIPO_ANALOGICO) v = (uint32_t)ulp_riscv_adc_read_channel(ADC_UNIT_1, (int)r->canale);
    else continue;
    s->valore = v;
    uint32_t vera = condizione_vera(r, v, s->vera);
    s->conta = vera ? s->conta + 1 : 0;
    uint32_t campioni = r->campioni ? r->campioni : 1;
    s->vera = vera && s->conta >= campioni;
    if (s->vera && !s->scattata) {
      s->scattata = 1;
      if (r->azione & ULP_AZ_SVEGLIA) sveglia = 1;
    }
    if (!s->vera) s->scattata = 0;
  }
  if (sveglia) ulp_riscv_wakeup_main_processor();
  return 0;
}
