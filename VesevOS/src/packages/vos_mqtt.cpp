// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_mqtt.cpp
// Usa il client MQTT di ESP-IDF (esp-mqtt, gia dentro il core Arduino-ESP32): nessuna libreria in piu.
// esp-mqtt ha un suo task interno; il nostro task "mqtt" invia lo stato e esegue i comandi in coda.
// Niente String qui dentro (restano solo ai confini: configurazione, rete, regole). Le parti senza rete stanno in vos_mqtt_msg.h.
#include <math.h>
#include "../drivers/vos_drv_task.h"
#include <string_view>
#include "vos_mqtt.h"
#include "vos_mqtt_msg.h"
#include "vos_mqtt_gate.h"
#include "../core/vos_eventbus.h"
#include "../drivers/vos_drv_heap.h"
#include "../core/vos_common.h"
#include "../core/vos_config.h"
#include "../core/vos_heapbuf.h"
#include "../core/vos_i18n.h"
#include "../core/vos_log.h"
#include "../core/vos_util.h"
#include "../core/vos_sys.h"
#include "../net/vos_net.h"
#include "vos_rules.h"
#include "../drivers/vos_drv_wifi.h"
#include "../drivers/vos_drv_mqtt.h"
#include "../drivers/vos_drv_heap.h"
#include "../core/vos_service.h"
#include "../core/vos_wd.h"
#include "../drivers/vos_drv_fs.h"
#include "../drivers/vos_drv_time.h"

#if VOS_WITH_MQTT
static volatile bool g_conn = false, g_onConnect = false, g_stopReq = false;
static volatile uint32_t g_sent = 0, g_recv = 0;
// ultimo errore come NUMERO: lo scrive anche il task del client MQTT e un numero si legge da un altro task senza rischi
// (un testo condiviso tra task puo rompere la memoria). Il testo tradotto si fa solo quando serve (errText).
enum { ME_NONE = 0, ME_REFUSED, ME_UNREACH, ME_NOHOST, ME_NOMEM, ME_LONG };
static volatile uint8_t g_err = ME_NONE;
static const char* errText() {
  switch (g_err) {
    case ME_REFUSED: return tr("Broker: accesso rifiutato (utente o password?)");
    case ME_UNREACH: return tr("Broker non raggiungibile");
    case ME_NOHOST: return tr("Manca l'indirizzo del broker");
    case ME_NOMEM: return tr("Memoria insufficiente");
    case ME_LONG: return tr("Valori troppo lunghi");
    default: return "";
  }
}

// Un solo blocco di memoria (PSRAM se c'e) vivo finche il client esiste: coda dei comandi, spazio di lavoro e testi di configurazione.
// esp-mqtt copia indirizzo, utente, password, identificativo e testamento, ma NON il certificato (tiene il puntatore): per questo
// tutto resta vivo fino alla fine del client. Utente e password si azzerano prima di liberare il blocco.
#define QN 4              // comandi in coda
#define QCAP 201          // un comando: al massimo 200 caratteri + zero
#define MQTT_CA_MAX 32768 // come il gestore dei file
struct MqttRt {
  char q[QN][QCAP]; int qn;       // coda dei comandi: la riempie il task del client, la svuota il task "mqtt" (sotto g_mx)
  char scratch[1024];             // stato JSON e messaggi di Home Assistant (solo il task "mqtt")
  char dev[352], av[208];         // pezzi comuni dei messaggi di Home Assistant
  const char *uri, *user, *pass, *id, *will, *cmd, *ca;   // dentro tail (cmd = <prefisso>/cmd: argomento dei comandi)
  char tail[1];
};
static MqttRt* volatile g_rt = nullptr;
static MqttDrvConfig g_dc;       // la riempie mqttStart; il task apre il client solo quando la rete e pronta
static SemaphoreHandle_t g_mx = NULL;

static void freeRt() {
  MqttRt* r = g_rt; g_rt = nullptr;
  if (!r) return;
  mqttSecureZero((void*)r->user, strlen(r->user));
  mqttSecureZero((void*)r->pass, strlen(r->pass));
  free(r);
}

// Identificativo della scheda ("vesevos_<mac>"). Il MAC arriva dalla rete come String (confine): si usa qui e si lascia subito.
static void deviceId(StrBuf& out) { char mac[20]; netMacTo(false, mac, sizeof mac); mqttDevId(mac, out); }

static MqttPrefix prefixNow() { return mqttPrefixParts(cfg.mqttPrefix.view(), cfg.hostname.view()); }

bool mqttRunning() { return sysTaskRunning("mqtt"); }
bool mqttConnected() { return g_conn; }

// Messaggio sull'argomento <prefisso>/<sub>: l'argomento si costruisce in un blocco di misura esatta (puo chiamare qualunque task,
// anche il web server: niente buffer sullo stack).
bool mqttPublishRel(const char* sub, const char* payload, bool retain) {
  if (!drvMqttIsOpen() || !g_conn || !sub || !payload) return false;
  MqttPrefix pv = prefixNow();
  size_t n = pv.size() + 1 + strlen(sub) + 1;
  char* t = (char*)malloc(n);
  if (!t) return false;
  StrBuf tb(t, n); tb.addn(pv.a.data(), pv.a.size()); tb.addn(pv.b.data(), pv.b.size()); tb.add('/'); tb.add(sub);
  bool ok = drvMqttPublish(t, payload, (int)strlen(payload), 0, retain);
  free(t);
  if (ok) g_sent = g_sent + 1;
  return ok;
}

static void pubAbs(const char* topic, const char* payload, size_t len, bool retain) {
  if (drvMqttPublish(topic, payload, (int)len, 1, retain)) g_sent = g_sent + 1;
}

// dal driver (task del client MQTT): si mettono da parte i dati, si lavora nel task del servizio
static void onConnect() { bool was = g_conn; g_conn = true; g_onConnect = true; g_err = ME_NONE; if (!was) EventBus::publish("mqtt.link", "", 1); }
static void onDisconnect() { bool was = g_conn; if (g_conn) vlog("MQTT: scollegato dal broker"); g_conn = false; if (was) EventBus::publish("mqtt.link", "", 0); }
static void onError(bool refused) { g_err = refused ? ME_REFUSED : ME_UNREACH; }
static void onData(const char* topic, int topicLen, const char* data, int dataLen, int totalLen) {
  g_recv = g_recv + 1;
  if (dataLen <= 0 || dataLen > 200 || totalLen != dataLen) return;   // solo comandi brevi, in un pezzo
  MqttRt* rt = g_rt;
  if (!rt) return;
  if (std::string_view(topic, topicLen > 0 ? (size_t)topicLen : 0) != std::string_view(rt->cmd)) return;   // topic non ha lo zero finale
  if (g_mx && xSemaphoreTake(g_mx, pdMS_TO_TICKS(50)) == pdTRUE) {
    if (rt->qn < QN) { mqttCmdClean(data, dataLen, rt->q[rt->qn]); rt->qn++; }
    xSemaphoreGive(g_mx);
  }
}

// Home Assistant: ogni valore diventa un sensore della stessa "scheda". Un messaggio che non entra nel buffer si scarta (mai a meta).
static void haDiscovery(MqttRt* rt) {
  char id[24]; StrBuf ib(id, sizeof id);
  deviceId(ib);
  MqttPrefix pv = prefixNow();
  StrBuf dev(rt->dev, sizeof rt->dev), av(rt->av, sizeof rt->av);
  mqttHaDev(dev, id, cfg.hostname.view(), VOS_BOARD, VOS_VERSION);
  mqttHaAv(av, pv);
  if (ib.truncated() || dev.truncated() || av.truncated()) { vlog("MQTT: Home Assistant: dati troppo lunghi, presentazione non inviata"); return; }
  struct S { const char* k; const char* n; const char* unit; const char* cls; };
  static const S list[] = {
    {"temp", "Temperatura CPU", "\xC2\xB0" "C", "temperature"}, {"cpu", "CPU", "%", ""},
    {"rssi", "Segnale Wi-Fi", "dBm", "signal_strength"}, {"heap", "RAM libera", "B", "data_size"},
    {"uptime", "Acceso da", "s", "duration"},
  };
  char topic[96];
  for (const S& x : list) {
    StrBuf c(rt->scratch, sizeof rt->scratch);
    mqttHaSensor(c, id, x.k, x.n, x.unit, x.cls, pv, rt->av, rt->dev);
    StrBuf tp(topic, sizeof topic); tp.add("homeassistant/sensor/"); tp.add(id); tp.add("/"); tp.add(x.k); tp.add("/config");
    if (c.truncated() || tp.truncated()) { vlog("MQTT: Home Assistant: sensore %s troppo lungo, non inviato", x.k); continue; }
    pubAbs(topic, rt->scratch, c.length(), true);
  }
  StrBuf b(rt->scratch, sizeof rt->scratch);
  mqttHaRebootButton(b, id, pv, rt->av, rt->dev);
  StrBuf tp(topic, sizeof topic); tp.add("homeassistant/button/"); tp.add(id); tp.add("/reboot/config");
  if (b.truncated() || tp.truncated()) { vlog("MQTT: Home Assistant: pulsante troppo lungo, non inviato"); return; }
  pubAbs(topic, rt->scratch, b.length(), true);
}

static void cleanup() {
  if (drvMqttIsOpen()) {
    MqttRt* rt = g_rt;
    if (g_conn && rt) { drvMqttPublish(rt->will, "offline", 7, 1, true); vTaskDelay(pdMS_TO_TICKS(200)); }
    drvMqttEnd();
  }
  g_conn = false;
  freeRt();      // dopo la fine del client: nessun callback puo piu leggere il blocco
}

// Il client MQTT si apre solo con la rete di casa pronta (mai dal setup(): lwIP non e ancora in piedi e getaddrinfo manda in panic).
// Se la rete cade per piu di qualche secondo il client si chiude (il blocco di configurazione resta); torna da solo con la rete.
static void netGate(MqttGate& gate, MqttRt* rt) {
  bool open = drvMqttIsOpen();
  switch (mqttGateStep(gate, netState() == NET_CLIENT_OK, open)) {
    case MG_START:
      if (drvMqttBegin(g_dc)) vlog("MQTT: rete pronta, avvio verso %s", rt->uri);
      else { g_err = ME_NOMEM; vlog("MQTT: memoria insufficiente, riprovo"); }
      break;
    case MG_STOP:
      vlog("MQTT: rete assente, client chiuso (riparte con la rete)");
      if (g_conn && rt) { g_conn = false; EventBus::publish("mqtt.link", "", 0); }
      drvMqttEnd();
      break;
    default: break;
  }
}

static void mqttTask(void*) {
  uint32_t lastState = 0;
  MqttGate gate;
  wdWatch("mqtt", wdLimit("mqtt"));                   // base 90 s
  for (;;) {
    wdBeat("mqtt");
    if (g_stopReq) { cleanup(); vlog("MQTT: fermato"); g_stopReq = false; wdUnwatch("mqtt"); vTaskDelete(NULL); }
    MqttRt* rt = g_rt;
    if (!rt) { wdUnwatch("mqtt"); vTaskDelete(NULL); }       // il blocco non c'e piu: non puo succedere, ma senza blocco non si lavora
    netGate(gate, rt);
    if (g_onConnect) {
      g_onConnect = false;
      vlog("MQTT: collegato a %s", cfg.mqttHost.c_str());
      pubAbs(rt->will, "online", 6, true);
      drvMqttSubscribe(rt->cmd, 1);
      if (cfg.mqttHa) haDiscovery(rt);
      lastState = 0;
    }
    char cmd[QCAP]; cmd[0] = 0;
    if (g_mx && xSemaphoreTake(g_mx, pdMS_TO_TICKS(20)) == pdTRUE) {
      if (rt->qn) { memcpy(cmd, rt->q[0], QCAP); memmove(rt->q[0], rt->q[1], (size_t)(rt->qn - 1) * QCAP); rt->qn--; }
      xSemaphoreGive(g_mx);
    }
    if (cmd[0]) {
      ErrBuf err;
      vlog("MQTT: comando '%s'", cmd);
      bool ok = rulesAction(cmd, err);
      mqttPublishRel("cmd/result", ok ? "ok" : err.c_str());
    }
    if (g_conn && (lastState == 0 || drvMillis() - lastState >= (uint32_t)cfg.mqttEvery * 1000UL)) {
      lastState = drvMillis(); if (!lastState) lastState = 1;
      char ip[16], mac[20]; netIpTo(ip, sizeof ip); netMacTo(false, mac, sizeof mac);
      StrBuf sb(rt->scratch, sizeof rt->scratch);
      mqttStateJson(sb, sysCpuPercent(), (int)lroundf(sysCpuTemp()), (uint32_t)drvHeapFree(), (uint32_t)drvPsramFree(),
                    (int)drvWifiRssi(), (uint32_t)sysUptimeSec(), ip, mac, VOS_VERSION);
      if (!sb.truncated()) mqttPublishRel("state", rt->scratch);
    }
    vTaskDelay(pdMS_TO_TICKS(200));
  }
}

void mqttStart() {
  if (mqttRunning()) mqttStop();
  if (!g_mx) g_mx = xSemaphoreCreateMutex();
  if (!cfg.mqttHost.length()) { g_err = ME_NOHOST; vlog("MQTT: manca l'indirizzo del broker"); return; }
  // misure: ogni testo ha il suo spazio esatto nel blocco (nessun limite inventato per host, utente o password)
  const char* scheme = cfg.mqttTls ? "mqtts://" : "mqtt://";
  char port[8]; snprintf(port, sizeof port, "%u", (unsigned)cfg.mqttPort);
  size_t nUri = strlen(scheme) + cfg.mqttHost.length() + 1 + strlen(port);
  size_t nUser = cfg.mqttUser.length(), nPass = cfg.mqttPass.length();
  MqttPrefix pv = prefixNow();
  size_t nWill = pv.size() + 7, nCmd = pv.size() + 4;               // + "/status", + "/cmd"
  char idb[24]; StrBuf ib(idb, sizeof idb);
  deviceId(ib);
  VosFile f; size_t nCa = 0;
  if (cfg.mqttTls) {
    f = drvFsOpen("/mqtt-ca.pem", "r");
    if (f) { nCa = f.size(); if (nCa > MQTT_CA_MAX) { f.close(); g_err = ME_LONG; return; } }
  }
  size_t n = sizeof(MqttRt) + (nUri + 1) + (nUser + 1) + (nPass + 1) + (ib.length() + 1) + (nWill + 1) + (nCmd + 1) + (nCa + 1);
  MqttRt* rt = (MqttRt*)drvCallocBig(n);
  if (!rt) { if (f) f.close(); g_err = ME_NOMEM; return; }
  char* w = rt->tail;
  rt->uri = w;  snprintf(w, nUri + 1, "%s%s:%s", scheme, cfg.mqttHost.c_str(), port); w += nUri + 1;
  rt->user = w; memcpy(w, cfg.mqttUser.c_str(), nUser); w += nUser + 1;
  rt->pass = w; memcpy(w, cfg.mqttPass.c_str(), nPass); w += nPass + 1;
  rt->id = w;   memcpy(w, idb, ib.length()); w += ib.length() + 1;
  rt->will = w; { StrBuf wb(w, nWill + 1); wb.addn(pv.a.data(), pv.a.size()); wb.addn(pv.b.data(), pv.b.size()); wb.add("/status"); } w += nWill + 1;
  rt->cmd = w;  { StrBuf cb(w, nCmd + 1); cb.addn(pv.a.data(), pv.a.size()); cb.addn(pv.b.data(), pv.b.size()); cb.add("/cmd"); } w += nCmd + 1;
  rt->ca = w;
  if (f) { size_t got = f.read((uint8_t*)w, nCa); f.close(); w[got] = 0; }
  g_rt = rt;
  g_dc = { rt->uri, rt->id, rt->user, rt->pass, rt->will, "offline", rt->ca, cfg.mqttTls, 30, 10000,
           onConnect, onDisconnect, onData, onError };
  g_stopReq = false;
  // il client NON si apre qui: lo apre il task quando la rete e pronta (anche subito, se lo e gia)
  if (!drvTaskCreate(mqttTask, "mqtt", 6144, NULL, 1, NULL, DRV_CORE_ANY)) {   // core: automatico (prima: core 1 fisso)
    freeRt(); g_err = ME_NOMEM; return; }
  vlog("MQTT: configurato per %s (argomenti %.*s%.*s/...), parte con la rete", rt->uri, (int)pv.a.size(), pv.a.data(), (int)pv.b.size(), pv.b.data());
}

void mqttStop() {
  if (!mqttRunning()) { cleanup(); return; }
  g_stopReq = true;
  for (int i = 0; i < 40 && mqttRunning(); i++) vTaskDelay(pdMS_TO_TICKS(100));   // max 4 s
  if (mqttRunning()) { TaskHandle_t h = xTaskGetHandle("mqtt"); if (h) vTaskDelete(h); cleanup(); g_stopReq = false; }
}

// ---- contratto dei servizi (vos_service) ----
static int  svcState() { return g_conn ? SVC_ON : mqttRunning() ? SVC_ON_PROBLEM : SVC_OFF; }
static bool svcBegin(ErrBuf& err) {
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

// Stato per la pagina: un solo blocco con tetto; se non entra, errore come nella rete tra schede (mai JSON a meta).
bool mqttStatusJson(HeapBuf& h) {
  MqttPrefix pv = prefixNow();
  auto str = [&](const char* key, std::string_view v) { h.add(key); h.add("\""); h.addJson(v.data(), v.size()); h.add("\""); };
  auto num = [&](const char* key, uint32_t v) { h.add(key); h.addUint(v); };
  auto flag = [&](const char* key, bool v) { h.add(key); h.add(v ? "true" : "false"); };
  flag("{\"run\":", mqttRunning()); flag(",\"conn\":", g_conn);
  flag(",\"auto\":", cfg.mqttAuto); str(",\"host\":", cfg.mqttHost.view()); num(",\"port\":", cfg.mqttPort);
  str(",\"user\":", cfg.mqttUser.view()); flag(",\"hasPass\":", cfg.mqttPass.length() > 0);
  str(",\"prefix\":", cfg.mqttPrefix.view());
  h.add(",\"prefixUsed\":\""); h.addJson(pv.a.data(), pv.a.size()); h.addJson(pv.b.data(), pv.b.size()); h.add("\"");
  num(",\"every\":", cfg.mqttEvery); flag(",\"ha\":", cfg.mqttHa); flag(",\"tls\":", cfg.mqttTls); flag(",\"hasCa\":", drvFsExists("/mqtt-ca.pem"));
  num(",\"sent\":", g_sent); num(",\"recv\":", g_recv);
  const char* et = errText(); str(",\"err\":", std::string_view(et, strlen(et)));
  return h.add("}");
}

std::string mqttStatusText() {
  MqttPrefix pv = prefixNow();
  char* pf = (char*)malloc(pv.size() + 1);
  if (pf) mqttPrefixCopy(pv, pf, pv.size() + 1);
  StrText t(640 + cfg.mqttHost.length() + 2 * pv.size());
  StrBuf& o = t.buf();
  o.addf(tr("MQTT: %s, %s"), mqttRunning() ? tr("attivo") : tr("fermo"), g_conn ? tr("collegato") : tr("non collegato")); o.add('\n');
  o.addf(tr("Broker: %s:%u  avvio automatico: %s"), cfg.mqttHost.length() ? cfg.mqttHost.c_str() : "-", (unsigned)cfg.mqttPort, cfg.mqttAuto ? "si" : "no"); o.add('\n');
  o.addf(tr("Argomenti: %s/state, %s/cmd"), pf ? pf : "-", pf ? pf : "-"); o.add('\n');
  o.addf(tr("Inviati %lu, ricevuti %lu"), (unsigned long)g_sent, (unsigned long)g_recv); o.add('\n');
  if (g_err != ME_NONE) { o.addf(tr("Ultimo errore: %s"), errText()); o.add('\n'); }
  free(pf);
  return t.take();
}
#endif  // VOS_WITH_MQTT
