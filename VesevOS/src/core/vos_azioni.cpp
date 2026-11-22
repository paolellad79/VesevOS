// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_azioni.cpp
#include "vos_azioni.h"
#include "vos_common.h"
#include "vos_eventbus.h"
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include <stdio.h>
#include <string.h>

static const char* const kCat[AZ_N] = {"uscita", "pwm", "led", "contatore", "programma", "riavvio", "sonno", "avvio", "rete", "servizio", "sd"};
static const char* const kOrig[AO_N] = {"sistema", "shell", "pagina", "script", "regola"};

const char* azioneNomeCat(int cat) { return cat >= 0 && cat < AZ_N ? kCat[cat] : "?"; }
const char* azioneNomeOrigine(int o) { return o >= 0 && o < AO_N ? kOrig[o] : "sistema"; }

int azioneCatDaTopic(const char* topic) {
  if (!topic || strncmp(topic, "act.", 4) != 0) return -1;
  for (int i = 0; i < AZ_N; i++) if (strcmp(topic + 4, kCat[i]) == 0) return i;
  return -1;
}

// Origine per task: poche voci (i task che agiscono sono pochi). Un blocco di chiusura breve, creato in modo statico.
#define AZ_TASKS 8
static StaticSemaphore_t g_mxBuf;
static SemaphoreHandle_t g_mx = xSemaphoreCreateMutexStatic(&g_mxBuf);
static TaskHandle_t      g_task[AZ_TASKS];
static uint8_t           g_orig[AZ_TASKS];

static int origineCorrente() {
  TaskHandle_t me = xTaskGetCurrentTaskHandle();
  int o = AO_SISTEMA;
  xSemaphoreTake(g_mx, portMAX_DELAY);
  for (int i = 0; i < AZ_TASKS; i++) if (g_task[i] == me) { o = g_orig[i]; break; }
  xSemaphoreGive(g_mx);
  return o;
}

static void impostaOrigine(int o) {
  TaskHandle_t me = xTaskGetCurrentTaskHandle();
  xSemaphoreTake(g_mx, portMAX_DELAY);
  int libero = -1, mio = -1;
  for (int i = 0; i < AZ_TASKS; i++) {
    if (g_task[i] == me) mio = i;
    else if (!g_task[i] && libero < 0) libero = i;
  }
  if (o == AO_SISTEMA) {                                // si libera la voce
    if (mio >= 0) { g_task[mio] = nullptr; g_orig[mio] = 0; }
  } else {
    int i = mio >= 0 ? mio : libero;
    if (i >= 0) { g_task[i] = me; g_orig[i] = (uint8_t)o; }   // tabella piena: l'origine resta "sistema"
  }
  xSemaphoreGive(g_mx);
}

static volatile uint16_t g_mask = 0;             // categorie che il registratore vuole adesso (0 = nessuna)
void azioniImposta(uint16_t mascheraCategorie) { g_mask = mascheraCategorie; }

void azioneOrigineImposta(int origine) { impostaOrigine(origine); }

void azioneOrigineLibera(void* task) {
  xSemaphoreTake(g_mx, portMAX_DELAY);
  for (int i = 0; i < AZ_TASKS; i++) if (g_task[i] == (TaskHandle_t)task) { g_task[i] = nullptr; g_orig[i] = 0; }
  xSemaphoreGive(g_mx);
}

AzioneDa::AzioneDa(int origine, bool soloSeSistema) : prev_(origineCorrente()) {
  if (!soloSeSistema || prev_ == AO_SISTEMA) impostaOrigine(origine);
}
AzioneDa::~AzioneDa() { impostaOrigine(prev_); }

// Valore dell'evento: byte alto = origine, 24 bit bassi = numero (0..16777215; i negativi diventano 0).
void azionePubblica(int cat, const char* nome, int32_t valore) {
#if VOS_WITH_REC
  if (cat < 0 || cat >= AZ_N || !(g_mask & (1u << cat))) return;
  char topic[16];
  snprintf(topic, sizeof topic, "act.%s", kCat[cat]);
  uint32_t v = valore < 0 ? 0u : (uint32_t)valore;
  if (v > 0xFFFFFFu) v = 0xFFFFFFu;
  EventBus::publish(topic, nome ? nome : "", (int32_t)(((uint32_t)origineCorrente() << 24) | v));
#else
  (void)cat; (void)nome; (void)valore;
#endif
}

void azioneDecodifica(int32_t val, int& origine, int32_t& valore) {
  origine = (int)(((uint32_t)val >> 24) & 0xFF);
  valore = (int32_t)((uint32_t)val & 0xFFFFFFu);
}
