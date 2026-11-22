// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_drv_sonno.cpp
#include "vos_drv_sonno.h"
#include "vos_drv_ulp.h"
#include "../boards/vos_board.h"
#include <esp_sleep.h>
#include "vos_drv_gpio.h"
#include "vos_drv_time.h"

static int g_ripPin = -1;
static bool g_ripAlto = false;

void drvSonnoRipresa(int pin, bool alto) {
  g_ripPin = pin;
  g_ripAlto = alto;
}

bool drvSonnoRipresaPresente() {
  return g_ripPin >= 0;
}

static int causaSonno() {
  switch (esp_sleep_get_wakeup_cause()) {
    case ESP_SLEEP_WAKEUP_UNDEFINED: return SONNO_CAUSA_NESSUNA;
    case ESP_SLEEP_WAKEUP_TIMER:     return SONNO_CAUSA_TIMER;
    case ESP_SLEEP_WAKEUP_GPIO:
    case ESP_SLEEP_WAKEUP_EXT0:
    case ESP_SLEEP_WAKEUP_EXT1:      return SONNO_CAUSA_PIEDINO;
    default:                         return SONNO_CAUSA_ALTRA;
  }
}

bool drvSonnoRipresaPremuta() {
  if (g_ripPin < 0) return false;
  return drvGpioRead(g_ripPin) == g_ripAlto;
}

bool drvSonnoLeggero(uint32_t sec, DrvSonnoEsito& esito) {
  int pin = g_ripPin;
  bool alto = g_ripAlto;
  esito.codice = 3;
  esito.causa = SONNO_CAUSA_NESSUNA;
  esito.durataMs = 0;
  esito.livelloPrima = -1;
  esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
  if (drvUlpAttivo()) drvUlpPreparaSonno();        // la riga sopra spegne anche il risveglio dall'ULP: si riarma
  if (sec) esp_sleep_enable_timer_wakeup((uint64_t)sec * 1000000ULL);
  bool usaExt1 = false;
  if (pin >= 0) {
    drvGpioMode(pin, alto ? DG_INPUT_PULLDOWN : DG_INPUT_PULLUP);
    // Per i piedini con funzione RTC nel sonno vale la resistenza RTC, non quella digitale: senza, il piedino resta in aria e scatta subito.
    drvGpioRtcResistenza(pin, alto);
    esito.livelloPrima = drvGpioRead(pin) ? 1 : 0;                  // letto DOPO la resistenza: e il livello che il sonno vedra
#if !CONFIG_IDF_TARGET_ESP32C3
    // Sui chip con EXT1 il piedino RTC sveglia anche il sonno leggero con EXT1 (guida ESP-IDF 5.5.5, sleep_modes).
    if (drvGpioHaRtc(pin)) {
      esp_sleep_enable_ext1_wakeup_io(1ULL << pin, alto ? ESP_EXT1_WAKEUP_ANY_HIGH : ESP_EXT1_WAKEUP_ANY_LOW);
      usaExt1 = true;
    }
#endif
    if (!usaExt1) {
      drvGpioRisveglioLivello(pin, alto);
      esp_sleep_enable_gpio_wakeup();
    }
  }
  unsigned long t0 = drvMillis();
  esp_err_t e = esp_light_sleep_start();      // ritorna al risveglio; un errore vuol dire che il sonno non e partito
  esito.durataMs = (uint32_t)(drvMillis() - t0);
  esito.causa = causaSonno();
  if (pin >= 0) {
    if (usaExt1) {
      // dopo il risveglio il piedino e in hold: lo si sblocca e lo si riporta digitale
      drvGpioRtcRilascia(pin);
      drvGpioMode(pin, alto ? DG_INPUT_PULLDOWN : DG_INPUT_PULLUP);
    } else {
      drvGpioRisveglioSpegni(pin);
    }
  }
  if (e == ESP_OK) esito.codice = 0;
  else if (e == ESP_ERR_SLEEP_REJECT) esito.codice = 1;
  else if (e == ESP_ERR_SLEEP_TOO_SHORT_SLEEP_DURATION) esito.codice = 2;
  return e == ESP_OK;
}

bool drvSonnoRisvegliatoDaTimer() {
  return esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_TIMER;
}

void drvSonnoProfondo(uint32_t sec, bool conRipresa) {
  int pin = conRipresa ? g_ripPin : -1;
  bool alto = g_ripAlto;
  esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
  if (drvUlpAttivo()) drvUlpPreparaSonno();        // la riga sopra spegne anche il risveglio dall'ULP: si riarma
  if (sec) esp_sleep_enable_timer_wakeup((uint64_t)sec * 1000000ULL);
  if (pin >= 0) {
    uint64_t maschera = 1ULL << pin;
#if CONFIG_IDF_TARGET_ESP32C3
    // C3: nessun EXT1; il risveglio da piedino nel sonno profondo e questo (solo GPIO0-5, guida ESP-IDF 5.5.5)
    esp_deep_sleep_enable_gpio_wakeup(maschera, alto ? ESP_GPIO_WAKEUP_GPIO_HIGH : ESP_GPIO_WAKEUP_GPIO_LOW);
#else
    esp_sleep_enable_ext1_wakeup_io(maschera, alto ? ESP_EXT1_WAKEUP_ANY_HIGH : ESP_EXT1_WAKEUP_ANY_LOW);
#endif
    // resistenza interna nel dominio RTC, cosi il piedino non resta in aria
    drvGpioRtcResistenza(pin, alto);
  }
  esp_deep_sleep_start();
}
