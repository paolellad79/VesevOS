// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_mqtt.cpp
// Usa il client MQTT di ESP-IDF (esp-mqtt, gia dentro il core Arduino-ESP32): nessuna libreria in piu.
// esp-mqtt ha un suo task interno; il nostro task "mqtt" invia lo stato e esegue i comandi in coda.
#include "vos_mqtt.h"
#include "../core/vos_eventbus.h"
#include "../core/vos_common.h"
#include "../core/vos_config.h"
#include "../core/vos_i18n.h"
#include "../core/vos_log.h"
#include "../core/vos_util.h"
#include "../core/vos_sys.h"
#include "../net/vos_net.h"
#include "vos_rules.h"
#include "../drivers/vos_drv_wifi.h"
#include "../drivers/vos_drv_mqtt.h"
#include "../core/vos_service.h"
#include "../core/vos_wd.h"
#include "../drivers/vos_drv_fs.h"

#if VOS_WITH_MQTT
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
  if (!drvMqttIsOpen() || !g_conn) return false;
  String t = mqttPrefix() + "/" + sub;
  bool ok = drvMqttPublish(t.c_str(), payload.c_str(), payload.length(), 0, retain);
  if (ok) g_sent++;
  return ok;
}

static void pubAbs(const String& topic, const String& payload, bool retain) {
  if (drvMqttPublish(topic.c_str(), payload.c_str(), payload.length(), 1, retain)) g_sent++;
}

// dal driver (task del client MQTT): si mettono da parte i dati, si lavora nel task del servizio
static void onConnect() { bool was = g_conn; g_conn = true; g_onConnect = true; g_lastErr = ""; if (!was) EventBus::publish("mqtt.link", "", 1); }
static void onDisconnect() { bool was = g_conn; if (g_conn) vlog("MQTT: scollegato dal broker"); g_conn = false; if (was) EventBus::publish("mqtt.link", "", 0); }
static void onError(bool refused) { g_lastErr = refused ? tr("Broker: accesso rifiutato (utente o password?)") : tr("Broker non raggiungibile"); }
static void onData(const char* topic, int topicLen, const char* data, int dataLen, int totalLen) {
  g_recv++;
  if (dataLen <= 0 || dataLen > 200 || totalLen != dataLen) return;   // solo comandi brevi, in un pezzo
  String t = String(topic).substring(0, topicLen);
  if (t != mqttPrefix() + "/cmd") return;
  String p; p.reserve(dataLen);
  for (int i = 0; i < dataLen; i++) { char c = data[i]; p += (c >= 32 && c < 127) ? c : ' '; }
  if (g_mx && xSemaphoreTake(g_mx, pdMS_TO_TICKS(50)) == pdTRUE) {
    if (g_qn < QN) g_q[g_qn++] = p;
    xSemaphoreGive(g_mx);
  }
}

static String stateJson() {
  String j = "{";
  j += "\"cpu\":" + String(sysCpuPercent());
  j += ",\"temp\":" + String(sysCpuTemp(), 1);
  j += ",\"heap\":" + String((unsigned long)ESP.getFreeHeap());
  j += ",\"psram\":" + String((unsigned long)ESP.getFreePsram());
  j += ",\"rssi\":" + String((int)drvWifiRssi());
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
  if (drvMqttIsOpen()) {
    if (g_conn) { drvMqttPublish(g_will.c_str(), "offline", 7, 1, true); vTaskDelay(pdMS_TO_TICKS(200)); }
    drvMqttEnd();
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
      drvMqttSubscribe((mqttPrefix() + "/cmd").c_str(), 1);
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
  g_ca = "";
  if (cfg.mqttTls) {
    File f = drvFsOpen("/mqtt-ca.pem", "r");
    if (f) { g_ca = f.readString(); f.close(); }
  }
  MqttDrvConfig dc = { g_uri.c_str(), g_id.c_str(), g_user.c_str(), g_pass.c_str(), g_will.c_str(), "offline", g_ca.c_str(), cfg.mqttTls, 30, 10000,
                       onConnect, onDisconnect, onData, onError };
  if (!drvMqttBegin(dc)) { g_lastErr = tr("Memoria insufficiente"); return; }
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

// ---- contratto dei servizi (vos_service) ----
static int  svcState() { return g_conn ? SVC_ON : mqttRunning() ? SVC_ON_PROBLEM : SVC_OFF; }
static bool svcBegin(String& err) {
  if (!cfg.mqttHost.length()) { err = tr("Manca l'indirizzo del broker"); return false; }
  mqttStart(); return true;
}
static uint32_t svcRam() { return mqttRunning() ? 6144u + 12000u : 0u; }      // pila del task + client MQTT (stima)
static const ServiceOps MQTT_OPS = { "mqtt", "MQTT", "Messaggi", "mqtt", ROLE_OPER, ROLE_ADMIN,
                                     svcState, svcBegin, mqttStop, nullptr, mqttStatusJson, nullptr, svcRam };

void mqttInit() {
  serviceRegister(&MQTT_OPS);
  sysTaskRegister("mqtt", mqttStart, mqttStop);
  if (cfg.mqttAuto && cfg.mqttHost.length()) mqttStart();
}

String mqttStatusJson() {
  String j = "{\"run\":" + String(mqttRunning() ? "true" : "false") + ",\"conn\":" + String(g_conn ? "true" : "false");
  j += ",\"auto\":" + String(cfg.mqttAuto ? "true" : "false") + ",\"host\":\"" + jsonEscape(cfg.mqttHost) + "\",\"port\":" + String(cfg.mqttPort);
  j += ",\"user\":\"" + jsonEscape(cfg.mqttUser) + "\",\"hasPass\":" + String(cfg.mqttPass.length() ? "true" : "false");
  j += ",\"prefix\":\"" + jsonEscape(cfg.mqttPrefix) + "\",\"prefixUsed\":\"" + jsonEscape(mqttPrefix()) + "\"";
  j += ",\"every\":" + String(cfg.mqttEvery) + ",\"ha\":" + String(cfg.mqttHa ? "true" : "false") +
       ",\"tls\":" + String(cfg.mqttTls ? "true" : "false") + ",\"hasCa\":" + String(drvFsExists("/mqtt-ca.pem") ? "true" : "false");
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
#endif  // VOS_WITH_MQTT
