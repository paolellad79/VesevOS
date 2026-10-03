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
#include <Arduino.h>

void   mqttInit();                          // registra il servizio; parte se "avvio automatico" e acceso
void   mqttStart();                         // avvia (o riavvia) il client
void   mqttStop();                          // ferma il client e il task
bool   mqttRunning();                       // task attivo
bool   mqttConnected();                     // collegato al broker
bool   mqttPublishRel(const String& sub, const String& payload, bool retain = false);   // <prefisso>/<sub>
String mqttStatusJson();
String mqttStatusText();
String mqttPrefix();
