// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_drv_task.h
// STRATO 1 (driver): cedere il processore ai task di priorita piu bassa. Solo FreeRTOS, nessuna dipendenza da Arduino.
#pragma once
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_task_wdt.h>

// ---- CREAZIONE DEI TASK DI VESEVOS: core automatico (decisione del 22/10/2026) ----
// Tutti i task nostri nascono senza core fisso (tskNO_AFFINITY): li assegna FreeRTOS al core libero. Accanto a ogni creazione c'e il commento
// "core: automatico (prima: core N fisso)". PER TORNARE INDIETRO basta scrivere quel numero al posto di DRV_CORE_ANY nella chiamata
// (nessuna impostazione, nessun altro cambio). Elenco completo di com'era prima (versione 1.7.70):
//   task       file                      core prima
//   net        net/vos_net.cpp           0
//   wd         core/vos_wd.cpp           0
//   monitor    core/vos_sys.cpp          0
//   time       core/vos_time.cpp         0
//   rules      packages/vos_rules.cpp    0
//   tmpclean   core/vos_tmpclean.cpp     0
//   ntp, dns   drivers/vos_drv_udp.cpp   0   (il server UDP: un solo punto per i due)
//   loopTask   vos_main.cpp              1   (drvMainTaskStart, drivers/vos_drv_boot.cpp)
//   berry      packages/vos_berry.cpp    1
//   mqtt       packages/vos_mqtt.cpp     1
//   mesh       packages/vos_mesh.cpp     1
//   meshst     packages/vos_mesh.cpp     1   (la lambda che avvia la mesh dopo il Wi-Fi)
//   led        devices/vos_led.cpp       1
//   selftest   packages/vos_selftest.cpp 1
// Gia' automatici (xTaskCreate senza core, lasciati cosi): rst e slp (riavvio e sospensione ritardati: core/vos_wd.cpp, interfaces/vos_web_sys.cpp).
// NON toccati (sono di ESP-IDF e del suo sdkconfig, hanno il core scelto da Espressif per i driver radio e lo stack di rete):
//   main (app_main), esp_timer, Wi-Fi, BLE (NimBLE e controller), tcpip_thread di lwIP, httpd di PsychicHttp, esp_mqtt, mDNS, IDLE0/IDLE1, ipc, Tmr Svc.
constexpr int DRV_CORE_ANY = tskNO_AFFINITY;

// Crea un task. core: DRV_CORE_ANY (automatico) oppure 0/1 (fisso). Ritorna false se non c'e memoria.
inline bool drvTaskCreate(void (*fn)(void*), const char* name, uint32_t stackBytes, void* arg, UBaseType_t priority, TaskHandle_t* handle, int core) {
  return xTaskCreatePinnedToCore(fn, name, stackBytes, arg, priority, handle, core) == pdPASS;
}

// Pausa dentro i calcoli lunghi (hash delle password, misure). Fa due cose:
//  1) il task dorme per un tick: cosi il task "idle" del core puo girare e dare il segnale al suo watchdog;
//  2) alimenta il watchdog del task chiamante, se e iscritto (il loopTask lo e: senza questo scatta dopo 5 s anche con la pausa).
// taskYIELD() non basta: cede solo a priorita pari o piu alta. Per un task non iscritto (es. server web) esp_task_wdt_reset()
// risponde ESP_ERR_NOT_FOUND e ESP-IDF scrive un errore nel log a ogni chiamata: si controlla prima con esp_task_wdt_status().
inline void drvTaskPause() {
  vTaskDelay(1);
  if (esp_task_wdt_status(nullptr) == ESP_OK) (void)esp_task_wdt_reset();
}

// Il task chiamante scende alla priorita del task "idle" finche l'oggetto vive, poi torna come prima. Per lavori di fondo lunghi (pulizie,
// calcoli sul file system) che non devono togliere tempo a nessuno: l'idle gira a turno e il Task watchdog non scatta.
class DrvIdlePriority {
 public:
  DrvIdlePriority() : me_(xTaskGetCurrentTaskHandle()), prio_(uxTaskPriorityGet(me_)) { vTaskPrioritySet(me_, tskIDLE_PRIORITY); }
  ~DrvIdlePriority() { vTaskPrioritySet(me_, prio_); }
  DrvIdlePriority(const DrvIdlePriority&) = delete;
  DrvIdlePriority& operator=(const DrvIdlePriority&) = delete;
 private:
  TaskHandle_t me_;
  UBaseType_t prio_;
};
