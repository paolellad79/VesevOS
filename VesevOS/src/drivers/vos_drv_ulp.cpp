// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_drv_ulp.cpp
// Il programma del coprocessore sta in VesevOS/ulp/ (fuori da src/); lo compila ulp_embed_binary (idf/main/CMakeLists.txt) e lo mette dentro
// il firmware. Questo file lo carica, scrive la tabella e lo avvia.
#include "vos_drv_ulp.h"
#include "vos_drv_gpio.h"
#include "../boards/vos_packages.h"

#if VOS_WITH_ULP_SOGLIE
#include "../../ulp/vos_ulp_tabella.h"
#include "ulp_vos_soglie.h"        // generato da ulp_embed_binary: esporta ulp_tabella (variabile del coprocessore)
#include <ulp_riscv.h>
#include <ulp_adc.h>
#include <esp_sleep.h>
#include <string.h>

static_assert(DRV_ULP_RIGHE_MAX == ULP_RIGHE_MAX, "righe del driver e della tabella condivisa diverse");

extern const uint8_t ulp_vos_soglie_bin_start[] asm("_binary_ulp_vos_soglie_bin_start");
extern const uint8_t ulp_vos_soglie_bin_end[]   asm("_binary_ulp_vos_soglie_bin_end");

static bool g_attivo = false;
static int  g_pinRtc[ULP_RIGHE_MAX];     // piedini RTC preparati per le righe a livello (da rilasciare al fermo)
static int  g_nPinRtc = 0;

static volatile vos_ulp_tabella_t* tabella() {
  return (volatile vos_ulp_tabella_t*)&ulp_tabella;
}

bool drvUlpDisponibile() { return true; }
int  drvUlpRigheMax() { return ULP_RIGHE_MAX; }
bool drvUlpAttivo() { return g_attivo; }

// Controlla le righe con il chip e ricava i canali. canale[i]: numero GPIO (livello) o canale ADC1 (analogico).
static int controllaRighe(const DrvUlpRiga* r, int n, uint32_t* canale, bool& haAnalogico, int& pinAnalogico) {
  int canaleAdc = -1;
  haAnalogico = false;
  pinAnalogico = -1;
  for (int i = 0; i < n; i++) {
    if (r[i].sorgente == DRV_ULP_LIVELLO) {
      if (!drvGpioHaRtc(r[i].piedino)) return DRV_ULP_E_PIEDINO;
      canale[i] = (uint32_t)r[i].piedino;
    } else if (r[i].sorgente == DRV_ULP_ANALOGICO) {
      int unita, ch;
      if (!drvGpioCanaleAdc(r[i].piedino, unita, ch)) return DRV_ULP_E_NON_ADC1;
      if (unita != 1) return DRV_ULP_E_NON_ADC1;
      if (canaleAdc >= 0 && canaleAdc != ch) return DRV_ULP_E_DUE_CANALI;
      canaleAdc = ch;
      haAnalogico = true;
      pinAnalogico = r[i].piedino;
      canale[i] = (uint32_t)ch;
    } else {
      return DRV_ULP_E_RIGA;
    }
    if (r[i].condizione < DRV_ULP_ALTO || r[i].condizione > DRV_ULP_DENTRO) return DRV_ULP_E_RIGA;
  }
  return DRV_ULP_OK;
}

// Prepara un piedino RTC in ingresso con la resistenza voluta, e lo "congela" (hold) per il sonno.
static void preparaPiedino(int pin, int resistenza) {
  drvGpioRtcIngresso(pin, resistenza == DRV_ULP_RES_SU ? DG_RES_SU : (resistenza == DRV_ULP_RES_GIU ? DG_RES_GIU : DG_RES_NESSUNA));
}

int drvUlpAvvia(const DrvUlpRiga* righe, int n, uint32_t periodoUs) {
  if (!righe || n < 0) return DRV_ULP_E_RIGA;
  if (n > ULP_RIGHE_MAX) return DRV_ULP_E_TROPPE_RIGHE;
  uint32_t canale[ULP_RIGHE_MAX];
  bool haAnalogico;
  int pinAnalogico;
  int e = controllaRighe(righe, n, canale, haAnalogico, pinAnalogico);
  if (e) return e;
  if (g_attivo) drvUlpFerma();
  // 1) piedini e ADC, PRIMA di caricare il programma
  for (int i = 0; i < n; i++) {
    if (righe[i].sorgente != DRV_ULP_LIVELLO) continue;
    preparaPiedino(righe[i].piedino, righe[i].resistenza);
    g_pinRtc[g_nPinRtc++] = righe[i].piedino;
  }
  if (haAnalogico) {
    int unita, ch;
    drvGpioCanaleAdc(pinAnalogico, unita, ch);       // gia controllato sopra
    ulp_adc_cfg_t cfg = {};
    cfg.adc_n = ADC_UNIT_1;
    cfg.channel = (adc_channel_t)ch;
    cfg.atten = ADC_ATTEN_DB_12;
    cfg.width = ADC_BITWIDTH_DEFAULT;
    cfg.ulp_mode = ADC_ULP_MODE_RISCV;
    if (ulp_adc_init(&cfg) != ESP_OK) {
      drvUlpFerma();
      return DRV_ULP_E_ADC;
    }
  }
  // 2) programma, tabella, periodo, avvio
  if (ulp_riscv_load_binary(ulp_vos_soglie_bin_start, ulp_vos_soglie_bin_end - ulp_vos_soglie_bin_start) != ESP_OK) {
    drvUlpFerma();
    return DRV_ULP_E_CARICAMENTO;
  }
  volatile vos_ulp_tabella_t* t = tabella();
  memset((void*)t, 0, sizeof(vos_ulp_tabella_t));
  for (int i = 0; i < n; i++) {
    t->righe[i].canale = canale[i];
    t->righe[i].condizione = (uint32_t)righe[i].condizione;
    t->righe[i].soglia1 = righe[i].soglia1;
    t->righe[i].soglia2 = righe[i].soglia2;
    t->righe[i].isteresi = righe[i].isteresi;
    t->righe[i].campioni = righe[i].campioni;
    t->righe[i].azione = righe[i].azione;
    t->righe[i].tipo = righe[i].sorgente == DRV_ULP_LIVELLO ? ULP_TIPO_LIVELLO : ULP_TIPO_ANALOGICO;   // per ultimo: il tipo "accende" la riga
  }
  ulp_set_wakeup_period(0, periodoUs);
  if (ulp_riscv_run() != ESP_OK) {
    drvUlpFerma();
    return DRV_ULP_E_AVVIO;
  }
  g_attivo = true;
  return DRV_ULP_OK;
}

bool drvUlpStato(int i, DrvUlpStato& s) {
  if (!g_attivo || i < 0 || i >= ULP_RIGHE_MAX) return false;
  volatile vos_ulp_tabella_t* t = tabella();
  s.valore = t->stato[i].valore;
  s.vera = t->stato[i].vera != 0;
  s.conta = t->stato[i].conta;
  s.scattata = t->stato[i].scattata != 0;
  s.cicli = t->cicli;
  return true;
}

void drvUlpPreparaSonno() {
  if (!g_attivo) return;
  esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_ON);   // tiene la configurazione dell'ADC e dei piedini RTC in sonno
  esp_sleep_enable_ulp_wakeup();
}

void drvUlpFerma() {
  ulp_riscv_timer_stop();
  ulp_riscv_halt();
  for (int i = 0; i < g_nPinRtc; i++) {
    drvGpioRtcRilascia(g_pinRtc[i]);
  }
  g_nPinRtc = 0;
  g_attivo = false;
}

#else   // senza coprocessore: tutto "non disponibile"
bool drvUlpDisponibile() { return false; }
int  drvUlpRigheMax() { return 0; }
int  drvUlpAvvia(const DrvUlpRiga*, int, uint32_t) { return DRV_ULP_E_NON_DISPONIBILE; }
bool drvUlpStato(int, DrvUlpStato&) { return false; }
bool drvUlpAttivo() { return false; }
void drvUlpPreparaSonno() {}
void drvUlpFerma() {}
#endif
