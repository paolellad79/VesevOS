// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_mqtt.h
// Client MQTT (servizio "mqtt"): invia lo stato della scheda a un broker e riceve comandi.
// Argomenti (prefisso predefinito vesevos/<nome host>):
//   <prefisso>/status        online / offline (messaggio "testamento", conservato)
//   <prefisso>/state         JSON con CPU, temperatura, RAM, segnale, IP... ogni N secondi
//   <prefisso>/cmd           comandi in arrivo: le stesse azioni delle Automazioni (led-color ff0000, gpio 4 1, reboot...)
//   <prefisso>/cmd/result    risposta: ok oppure il messaggio di errore
// Home Assistant: se attivo, la scheda si presenta da sola (homeassistant/sensor/...).
#pragma once
#include <string>
#include "../core/vos_heapbuf.h"
#include "../drivers/vos_drv_string.h"
#include "../core/vos_common.h"

#define MQTT_JSON_MAX 3072
#if VOS_WITH_MQTT
void   mqttInit();                          // registra il servizio; parte se "avvio automatico" e acceso
void   mqttStart();                         // avvia (o riavvia) il client
void   mqttStop();                          // ferma il client e il task
bool   mqttRunning();                       // task attivo
bool   mqttConnected();                     // collegato al broker
bool   mqttPublishRel(const char* sub, const char* payload, bool retain = false);   // <prefisso>/<sub>
bool        mqttStatusJson(HeapBuf& out);   // false = manca la memoria o supera il tetto
std::string mqttStatusText();               // per la shell
#else
// Package spento (VOS_WITH_MQTT = 0): funzioni vuote, il resto del codice non cambia.
inline void   mqttInit() {}
inline void   mqttStart() {}
inline void   mqttStop() {}
inline bool   mqttRunning() { return false; }
inline bool   mqttConnected() { return false; }
inline bool   mqttPublishRel(const char*, const char*, bool = false) { return false; }
inline bool   mqttStatusJson(HeapBuf& out) { return out.add("{\"present\":false}"); }
inline std::string mqttStatusText() { return "MQTT: non presente in questo firmware"; }
#endif
