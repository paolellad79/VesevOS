// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_mqtt.cpp
// Usa il client MQTT di ESP-IDF (esp-mqtt, gia dentro il core Arduino-ESP32): nessuna libreria in piu.
// esp-mqtt ha un suo task interno; il nostro task "mqtt" invia lo stato e esegue i comandi in coda.
#include "vos_mqtt.h"
#include "vos_common.h"
#include "vos_config.h"
#include "vos_i18n.h"
#include "vos_log.h"
#include "vos_util.h"
#include "vos_sys.h"
#include "vos_net.h"
#include "vos_rules.h"
#include <WiFi.h>
#include "mqtt_client.h"
#include "esp_crt_bundle.h"
#include "vos_wd.h"
#include <LittleFS.h>

static esp_mqtt_client_handle_t g_cli = nullptr;
static volatile bool g_conn = false, g_onConnect = false, g_stopReq = false;
static volatile uint32_t g_sent = 0, g_recv = 0;
static String g_lastErr = "";
static String g_uri, g_user, g_pass, g_id, g_will, g_ca;   // restano vivi finche il client esiste
static SemaphoreHandle_t g_mx = NULL;
#define QN 4
static String g_q[QN]; static int g_qn = 0;

String mqttPrefix() {
  String p = cfg.mqttPrefix; p.trim();
  if (!p.length()) p = "vesevos/" + cfg.hostname;
  while (p.endsWith("/")) p.remove(p.length() - 1);
  return p;
}
static String devId() { String m = netMac(false); m.replace(":", ""); m.toLowerCase(); return "vesevos_" + m; }

bool mqttRunning() { return sysTaskRunning("mqtt"); }
bool mqttConnected() { return g_conn; }

bool mqttPublishRel(const String& sub, const String& payload, bool retain) {
  if (!g_cli || !g_conn) return false;
  String t = mqttPrefix() + "/" + sub;
  int id = esp_mqtt_client_publish(g_cli, t.c_str(), payload.c_str(), payload.length(), 0, retain ? 1 : 0);
  if (id >= 0) g_sent++;
  return id >= 0;
}

static void pubAbs(const String& topic, const String& payload, bool retain) {
  if (!g_cli) return;
  if (esp_mqtt_client_publish(g_cli, topic.c_str(), payload.c_str(), payload.length(), 1, retain ? 1 : 0) >= 0) g_sent++;
}

static void onEvent(void*, esp_event_base_t, int32_t id, void* data) {
  esp_mqtt_event_handle_t ev = (esp_mqtt_event_handle_t)data;
  switch ((esp_mqtt_event_id_t)id) {
    case MQTT_EVENT_CONNECTED: g_conn = true; g_onConnect = true; g_lastErr = ""; break;
    case MQTT_EVENT_DISCONNECTED: if (g_conn) vlog("MQTT: scollegato dal broker"); g_conn = false; break;
    case MQTT_EVENT_DATA: {
      g_recv++;
      if (ev->data_len <= 0 || ev->data_len > 200 || ev->total_data_len != ev->data_len) break;   // solo comandi brevi, in un pezzo
      String t = String(ev->topic).substring(0, ev->topic_len);
      if (t != mqttPrefix() + "/cmd") break;
      String p; p.reserve(ev->data_len);
      for (int i = 0; i < ev->data_len; i++) { char c = ev->data[i]; p += (c >= 32 && c < 127) ? c : ' '; }
      if (g_mx && xSemaphoreTake(g_mx, pdMS_TO_TICKS(50)) == pdTRUE) {
        if (g_qn < QN) g_q[g_qn++] = p;
        xSemaphoreGive(g_mx);
      }
      break;
    }
    case MQTT_EVENT_ERROR:
      if (ev && ev->error_handle) {
        if (ev->error_handle->error_type == MQTT_ERROR_TYPE_CONNECTION_REFUSED) g_lastErr = tr("Broker: accesso rifiutato (utente o password?)");
        else g_lastErr = tr("Broker non raggiungibile");
      }
      break;
    default: break;
  }
}

static String stateJson() {
  String j = "{";
  j += "\"cpu\":" + String(sysCpuPercent());
  j += ",\"temp\":" + String(sysCpuTemp(), 1);
  j += ",\"heap\":" + String((unsigned long)ESP.getFreeHeap());
  j += ",\"psram\":" + String((unsigned long)ESP.getFreePsram());
  j += ",\"rssi\":" + String((int)WiFi.RSSI());
  j += ",\"uptime\":" + String((unsigned long)sysUptimeSec());
  j += ",\"ip\":\"" + netIpString() + "\"";
  j += ",\"mac\":\"" + netMac(false) + "\"";
  j += ",\"version\":\"" VOS_VERSION "\"";
  return j + "}";
}

// Home Assistant: ogni valore diventa un sensore della stessa "scheda"
static void haDiscovery() {
  String id = devId(), pre = mqttPrefix();
  String dev = "\"dev\":{\"ids\":[\"" + id + "\"],\"name\":\"" + jsonEscape(cfg.hostname) + "\",\"mdl\":\"" VOS_BOARD "\",\"mf\":\"VesevOS\",\"sw\":\"" VOS_VERSION "\"}";
  String av = "\"avty_t\":\"" + pre + "/status\"";
  struct S { const char* k; const char* n; const char* unit; const char* cls; };
  static const S list[] = {
    {"temp", "Temperatura CPU", "\xC2\xB0" "C", "temperature"}, {"cpu", "CPU", "%", ""},
    {"rssi", "Segnale Wi-Fi", "dBm", "signal_strength"}, {"heap", "RAM libera", "B", "data_size"},
    {"uptime", "Acceso da", "s", "duration"},
  };
  for (const S& x : list) {
    String c = "{\"name\":\"" + String(x.n) + "\",\"uniq_id\":\"" + id + "_" + x.k + "\",\"stat_t\":\"" + pre + "/state\",\"val_tpl\":\"{{ value_json." + x.k + " }}\"";
    if (x.unit[0]) c += ",\"unit_of_meas\":\"" + String(x.unit) + "\"";
    if (x.cls[0]) c += ",\"dev_cla\":\"" + String(x.cls) + "\"";
    c += ",\"stat_cla\":\"measurement\"," + av + "," + dev + "}";
    pubAbs("homeassistant/sensor/" + id + "/" + x.k + "/config", c, true);
  }
  String b = "{\"name\":\"Riavvia\",\"uniq_id\":\"" + id + "_reboot\",\"cmd_t\":\"" + pre + "/cmd\",\"pl_prs\":\"reboot\",\"dev_cla\":\"restart\"," + av + "," + dev + "}";
  pubAbs("homeassistant/button/" + id + "/reboot/config", b, true);
}

static void cleanup() {
  if (g_cli) {
    if (g_conn) { esp_mqtt_client_publish(g_cli, g_will.c_str(), "offline", 7, 1, 1); vTaskDelay(pdMS_TO_TICKS(200)); }
    esp_mqtt_client_stop(g_cli);
    esp_mqtt_client_destroy(g_cli);
    g_cli = nullptr;
  }
  g_conn = false;
}

static void mqttTask(void*) {
  uint32_t lastState = 0;
  wdWatch("mqtt", 30);
  for (;;) {
    wdBeat("mqtt");
    if (g_stopReq) { cleanup(); vlog("MQTT: fermato"); g_stopReq = false; wdUnwatch("mqtt"); vTaskDelete(NULL); }
    if (g_onConnect) {
      g_onConnect = false;
      vlog("MQTT: collegato a %s", cfg.mqttHost.c_str());
      pubAbs(g_will, "online", true);
      esp_mqtt_client_subscribe(g_cli, (mqttPrefix() + "/cmd").c_str(), 1);
      if (cfg.mqttHa) haDiscovery();
      lastState = 0;
    }
    String cmd = "";
    if (g_mx && xSemaphoreTake(g_mx, pdMS_TO_TICKS(20)) == pdTRUE) {
      if (g_qn) { cmd = g_q[0]; for (int i = 1; i < g_qn; i++) g_q[i - 1] = g_q[i]; g_qn--; }
      xSemaphoreGive(g_mx);
    }
    if (cmd.length()) {
      String err;
      vlog("MQTT: comando '%s'", cmd.c_str());
      bool ok = rulesAction(cmd, err);
      mqttPublishRel("cmd/result", ok ? String("ok") : err);
    }
    if (g_conn && (lastState == 0 || millis() - lastState >= (uint32_t)cfg.mqttEvery * 1000UL)) {
      lastState = millis(); if (!lastState) lastState = 1;
      mqttPublishRel("state", stateJson());
    }
    vTaskDelay(pdMS_TO_TICKS(200));
  }
}

void mqttStart() {
  if (mqttRunning()) mqttStop();
  if (!g_mx) g_mx = xSemaphoreCreateMutex();
  if (!cfg.mqttHost.length()) { g_lastErr = tr("Manca l'indirizzo del broker"); vlog("MQTT: manca l'indirizzo del broker"); return; }
  g_uri = String(cfg.mqttTls ? "mqtts://" : "mqtt://") + cfg.mqttHost + ":" + String(cfg.mqttPort);
  g_user = cfg.mqttUser; g_pass = cfg.mqttPass; g_id = devId();
  g_will = mqttPrefix() + "/status";
  esp_mqtt_client_config_t c = {};
  c.broker.address.uri = g_uri.c_str();
  if (cfg.mqttTls) {
    // verifica del server: certificato caricato dall'utente (/mqtt-ca.pem) oppure le autorita pubbliche note
    g_ca = "";
    File f = LittleFS.open("/mqtt-ca.pem", "r");
    if (f) { g_ca = f.readString(); f.close(); }
    if (g_ca.indexOf("-----BEGIN CERTIFICATE-----") >= 0) c.broker.verification.certificate = g_ca.c_str();
    else c.broker.verification.crt_bundle_attach = esp_crt_bundle_attach;
  }
  c.credentials.client_id = g_id.c_str();
  if (g_user.length()) c.credentials.username = g_user.c_str();
  if (g_pass.length()) c.credentials.authentication.password = g_pass.c_str();
  c.session.last_will.topic = g_will.c_str();
  c.session.last_will.msg = "offline";
  c.session.last_will.msg_len = 7;
  c.session.last_will.qos = 1;
  c.session.last_will.retain = 1;
  c.session.keepalive = 30;
  c.network.reconnect_timeout_ms = 10000;
  g_cli = esp_mqtt_client_init(&c);
  if (!g_cli) { g_lastErr = tr("Memoria insufficiente"); return; }
  esp_mqtt_client_register_event(g_cli, (esp_mqtt_event_id_t)ESP_EVENT_ANY_ID, onEvent, NULL);
  esp_mqtt_client_start(g_cli);
  g_stopReq = false;
  xTaskCreatePinnedToCore(mqttTask, "mqtt", 6144, NULL, 1, NULL, 1);
  vlog("MQTT: avvio verso %s (argomenti %s/...)", g_uri.c_str(), mqttPrefix().c_str());
}

void mqttStop() {
  if (!mqttRunning()) { cleanup(); return; }
  g_stopReq = true;
  for (int i = 0; i < 40 && mqttRunning(); i++) vTaskDelay(pdMS_TO_TICKS(100));   // max 4 s
  if (mqttRunning()) { TaskHandle_t h = xTaskGetHandle("mqtt"); if (h) vTaskDelete(h); cleanup(); g_stopReq = false; }
}

void mqttInit() {
  sysTaskRegister("mqtt", mqttStart, mqttStop);
  if (cfg.mqttAuto && cfg.mqttHost.length()) mqttStart();
}

String mqttStatusJson() {
  String j = "{\"run\":" + String(mqttRunning() ? "true" : "false") + ",\"conn\":" + String(g_conn ? "true" : "false");
  j += ",\"auto\":" + String(cfg.mqttAuto ? "true" : "false") + ",\"host\":\"" + jsonEscape(cfg.mqttHost) + "\",\"port\":" + String(cfg.mqttPort);
  j += ",\"user\":\"" + jsonEscape(cfg.mqttUser) + "\",\"hasPass\":" + String(cfg.mqttPass.length() ? "true" : "false");
  j += ",\"prefix\":\"" + jsonEscape(cfg.mqttPrefix) + "\",\"prefixUsed\":\"" + jsonEscape(mqttPrefix()) + "\"";
  j += ",\"every\":" + String(cfg.mqttEvery) + ",\"ha\":" + String(cfg.mqttHa ? "true" : "false") +
       ",\"tls\":" + String(cfg.mqttTls ? "true" : "false") + ",\"hasCa\":" + String(LittleFS.exists("/mqtt-ca.pem") ? "true" : "false");
  j += ",\"sent\":" + String((unsigned long)g_sent) + ",\"recv\":" + String((unsigned long)g_recv) + ",\"err\":\"" + jsonEscape(g_lastErr) + "\"}";
  return j;
}

String mqttStatusText() {
  String t;
  t += trf("MQTT: %s, %s", mqttRunning() ? tr("attivo") : tr("fermo"), g_conn ? tr("collegato") : tr("non collegato")) + "\n";
  t += trf("Broker: %s:%u  avvio automatico: %s", cfg.mqttHost.length() ? cfg.mqttHost.c_str() : "-", (unsigned)cfg.mqttPort, cfg.mqttAuto ? "si" : "no") + "\n";
  t += trf("Argomenti: %s/state, %s/cmd", mqttPrefix().c_str(), mqttPrefix().c_str()) + "\n";
  t += trf("Inviati %lu, ricevuti %lu", (unsigned long)g_sent, (unsigned long)g_recv) + "\n";
  if (g_lastErr.length()) t += trf("Ultimo errore: %s", g_lastErr.c_str()) + "\n";
  return t;
}
