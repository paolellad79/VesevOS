// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_drv_time.h
// STRATO 1 (driver): tempo e attese. Il resto del codice non chiama millis()/micros()/delay(): passa da qui.
// Sopra ESP-IDF (esp_timer, FreeRTOS, esp_rom): niente Arduino.
#pragma once
#include <stdint.h>
#include "esp_timer.h"
#include "esp_rom_sys.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// Millisecondi dall'avvio (si azzera dopo circa 49 giorni: i confronti si fanno con la differenza, come prima).
inline unsigned long drvMillis() { return (unsigned long)(esp_timer_get_time() / 1000); }
// Microsecondi dall'avvio.
inline unsigned long drvMicros() { return (unsigned long)esp_timer_get_time(); }
// Attesa in millisecondi: lascia lavorare gli altri task (0 = cede il turno).
inline void drvDelayMs(uint32_t ms) {
  if (ms) vTaskDelay(pdMS_TO_TICKS(ms)); else taskYIELD();
}
// Attesa breve in microsecondi: blocca il task (solo per tempi molto piccoli).
inline void drvDelayUs(uint32_t us) { esp_rom_delay_us(us); }
