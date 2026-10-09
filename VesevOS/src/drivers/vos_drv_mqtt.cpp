// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_drv_mqtt.cpp
#include "vos_drv_mqtt.h"
#if VOS_WITH_MQTT
#include "mqtt_client.h"
#include "esp_crt_bundle.h"

static esp_mqtt_client_handle_t s_cli = nullptr;
static MqttDrvConfig s_cfg;

static void onEvent(void*, esp_event_base_t, int32_t id, void* data) {
  esp_mqtt_event_handle_t ev = (esp_mqtt_event_handle_t)data;
  switch ((esp_mqtt_event_id_t)id) {
    case MQTT_EVENT_CONNECTED: if (s_cfg.onConnect) s_cfg.onConnect(); break;
    case MQTT_EVENT_DISCONNECTED: if (s_cfg.onDisconnect) s_cfg.onDisconnect(); break;
    case MQTT_EVENT_DATA: if (s_cfg.onData && ev) s_cfg.onData(ev->topic, ev->topic_len, ev->data, ev->data_len, ev->total_data_len); break;
    case MQTT_EVENT_ERROR:
      if (s_cfg.onError && ev && ev->error_handle) s_cfg.onError(ev->error_handle->error_type == MQTT_ERROR_TYPE_CONNECTION_REFUSED);
      break;
    default: break;
  }
}

bool drvMqttBegin(const MqttDrvConfig& cf) {
  if (s_cli) drvMqttEnd();
  s_cfg = cf;
  esp_mqtt_client_config_t c = {};
  c.broker.address.uri = s_cfg.uri;
  if (s_cfg.tls) {
    // verifica del server: certificato dell'utente oppure le autorita pubbliche note
    if (s_cfg.caPem && strstr(s_cfg.caPem, "-----BEGIN CERTIFICATE-----")) c.broker.verification.certificate = s_cfg.caPem;
    else c.broker.verification.crt_bundle_attach = esp_crt_bundle_attach;
  }
  c.credentials.client_id = s_cfg.clientId;
  if (s_cfg.user && s_cfg.user[0]) c.credentials.username = s_cfg.user;
  if (s_cfg.pass && s_cfg.pass[0]) c.credentials.authentication.password = s_cfg.pass;
  c.session.last_will.topic = s_cfg.willTopic;
  c.session.last_will.msg = s_cfg.willMsg;
  c.session.last_will.msg_len = (int)strlen(s_cfg.willMsg);
  c.session.last_will.qos = 1;
  c.session.last_will.retain = 1;
  c.session.keepalive = s_cfg.keepaliveSec;
  c.network.reconnect_timeout_ms = s_cfg.reconnectMs;
  s_cli = esp_mqtt_client_init(&c);
  if (!s_cli) return false;
  esp_mqtt_client_register_event(s_cli, (esp_mqtt_event_id_t)ESP_EVENT_ANY_ID, onEvent, NULL);
  esp_mqtt_client_start(s_cli);
  return true;
}

void drvMqttEnd() {
  if (!s_cli) return;
  esp_mqtt_client_stop(s_cli);
  esp_mqtt_client_destroy(s_cli);
  s_cli = nullptr;
}

bool drvMqttIsOpen() { return s_cli != nullptr; }
bool drvMqttPublish(const char* topic, const char* payload, int len, int qos, bool retain) {
  if (!s_cli) return false;
  return esp_mqtt_client_publish(s_cli, topic, payload, len, qos, retain ? 1 : 0) >= 0;
}
bool drvMqttSubscribe(const char* topic, int qos) { return s_cli && esp_mqtt_client_subscribe(s_cli, topic, qos) >= 0; }
#endif
