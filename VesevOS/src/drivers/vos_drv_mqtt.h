// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_drv_mqtt.h
// STRATO 1 (driver): unico file che parla con il client MQTT del sistema (esp_mqtt_client_*) e con le autorita dei certificati.
// Sa: aprire/chiudere il collegamento a un broker, pubblicare, iscriversi a un argomento, avvisare di collegato/scollegato/dati/errore.
// Non conosce argomenti, comandi, Home Assistant: sono del servizio (vos_mqtt). Le stringhe passate devono restare vive finche il client esiste.
#pragma once
#include <Arduino.h>
#include "../core/vos_common.h"

#if VOS_WITH_MQTT
struct MqttDrvConfig {
  const char* uri;                 // mqtt://host:porta oppure mqtts://host:porta
  const char* clientId;
  const char* user;                // nullptr = nessun utente
  const char* pass;                // nullptr = nessuna password
  const char* willTopic;           // messaggio "testamento" (conservato)
  const char* willMsg;
  const char* caPem;               // nullptr o senza "BEGIN CERTIFICATE" = autorita pubbliche note (solo con mqtts)
  bool        tls;
  int         keepaliveSec;
  int         reconnectMs;
  void (*onConnect)();
  void (*onDisconnect)();
  void (*onData)(const char* topic, int topicLen, const char* data, int dataLen, int totalLen);   // task del client: solo copiare e uscire
  void (*onError)(bool refused);   // refused = il broker ha rifiutato utente/password
};

bool drvMqttBegin(const MqttDrvConfig& c);     // false = memoria insufficiente
void drvMqttEnd();                             // ferma e libera il client
bool drvMqttIsOpen();                          // il client esiste
bool drvMqttPublish(const char* topic, const char* payload, int len, int qos, bool retain);   // true = accettato
bool drvMqttSubscribe(const char* topic, int qos);
#endif
