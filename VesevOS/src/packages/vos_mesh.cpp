// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_mesh.cpp
#include "vos_mesh.h"
#include "../core/vos_eventbus.h"
#include "../core/vos_config.h"
#include "../core/vos_region.h"
#include "../net/vos_net.h"
#include "../core/vos_sys.h"
#include "../core/vos_log.h"
#include "../core/vos_i18n.h"
#include "../core/vos_util.h"
#include "vos_rules.h"
#include "vos_mqtt.h"
#include "../core/vos_audit.h"
#include "../core/vos_wd.h"
#include "../core/vos_time.h"
#include "../drivers/vos_drv_wifi.h"
#include "../drivers/vos_drv_espnow.h"
#include "../core/vos_service.h"
#include "mbedtls/md.h"

#if VOS_WITH_MESH
#define M_VER     1
#define M_TTL     3
#define M_MAXPAY  180
#define M_MACLEN  16
#define NODES_MAX 16
#define MSGS_MAX  10
#define SEEN_MAX  32
#define QLEN      8

enum { T_BEACON = 1, T_STATE = 2, T_CMD = 3, T_RESULT = 4, T_TEXT = 5 };

struct __attribute__((packed)) MeshHdr {
  uint8_t  m0, m1, ver, type, ttl, hops, role;
  char     cc[2];
  uint8_t  src[6], dst[6];
  uint16_t boot;
  uint32_t seq;
  uint8_t  len;
};

struct Node { uint8_t mac[6]; char name[33]; uint8_t role, hops; int8_t rssi; uint32_t last; uint16_t boot; uint32_t seq; String state; bool ccx; bool used; };
struct Msg { String from, text; String when; };
struct RxItem { uint8_t len; int8_t rssi; uint8_t data[sizeof(MeshHdr) + M_MAXPAY + M_MACLEN]; };

static Node g_nodes[NODES_MAX];
static Msg  g_msgs[MSGS_MAX]; static int g_msgN = 0;
static uint32_t g_seen[SEEN_MAX]; static int g_seenI = 0;
static QueueHandle_t g_q = nullptr;
static volatile bool g_run = false, g_stopReq = false;
static uint8_t g_me[6];
static uint32_t g_seq = 0;
static uint16_t g_boot = 0;
static uint8_t g_key[32];
static uint32_t g_lastBeacon = 0, g_lastState = 0, g_lastHop = 0;
static uint32_t g_sent = 0, g_recv = 0, g_bad = 0;
static const uint8_t BCAST[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

static bool meshReady() { return g_run; }
bool meshRunning() { return sysTaskRunning("mesh"); }

String meshNewKey() { return randomHex(32); }

static bool keyLoad() {
  if (cfg.meshKey.length() != 64) return false;
  for (int i = 0; i < 32; i++) g_key[i] = (uint8_t)strtoul(cfg.meshKey.substring(i * 2, i * 2 + 2).c_str(), NULL, 16);
  return true;
}

static String macStr(const uint8_t* m) { char b[18]; snprintf(b, sizeof(b), "%02X:%02X:%02X:%02X:%02X:%02X", m[0], m[1], m[2], m[3], m[4], m[5]); return String(b); }

static void sign(const uint8_t* buf, size_t len, uint8_t* out) {
  // ttl e hops cambiano a ogni salto: si firmano a zero
  uint8_t tmp[sizeof(MeshHdr) + M_MAXPAY];
  memcpy(tmp, buf, len);
  ((MeshHdr*)tmp)->ttl = 0; ((MeshHdr*)tmp)->hops = 0;
  unsigned char full[32];
  mbedtls_md_hmac(mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), g_key, 32, tmp, len, full);
  memcpy(out, full, M_MACLEN);
}

int meshChannel() { return drvEspNowChannel(); }

static void rawSend(uint8_t* buf, size_t len) {
  if (drvEspNowSend(buf, len)) g_sent++;
}

static bool send(uint8_t type, const uint8_t* dst, const String& payload) {
  if (!meshReady()) return false;
  uint8_t buf[sizeof(MeshHdr) + M_MAXPAY + M_MACLEN];
  MeshHdr* h = (MeshHdr*)buf;
  size_t pl = payload.length() > M_MAXPAY ? M_MAXPAY : payload.length();
  h->m0 = 'V'; h->m1 = 'M'; h->ver = M_VER; h->type = type; h->ttl = M_TTL; h->hops = 0; h->role = cfg.meshRole;
  h->cc[0] = cfg.country.length() == 2 ? cfg.country[0] : '0'; h->cc[1] = cfg.country.length() == 2 ? cfg.country[1] : '1';
  memcpy(h->src, g_me, 6); memcpy(h->dst, dst, 6);
  h->boot = g_boot; h->seq = ++g_seq; h->len = pl;
  memcpy(buf + sizeof(MeshHdr), payload.c_str(), pl);
  sign(buf, sizeof(MeshHdr) + pl, buf + sizeof(MeshHdr) + pl);
  rawSend(buf, sizeof(MeshHdr) + pl + M_MACLEN);
  return true;
}

// callback del Wi-Fi: solo copia in coda (si lavora nel task)
static void onRecv(const uint8_t* data, int len, int8_t rssi) {
  if (!g_q || len < (int)(sizeof(MeshHdr) + M_MACLEN) || len > (int)sizeof(RxItem::data)) return;
  if (data[0] != 'V' || data[1] != 'M') return;
  RxItem it; it.len = len; it.rssi = rssi;
  memcpy(it.data, data, len);
  xQueueSend(g_q, &it, 0);
}

static bool seen(const MeshHdr* h) {
  uint32_t id = ((uint32_t)h->src[4] << 24) ^ ((uint32_t)h->src[5] << 16) ^ ((uint32_t)h->boot << 20) ^ h->seq;
  for (int i = 0; i < SEEN_MAX; i++) if (g_seen[i] == id) return true;
  g_seen[g_seenI] = id; g_seenI = (g_seenI + 1) % SEEN_MAX;
  return false;
}

static Node* nodeOf(const uint8_t* mac, bool create) {
  Node* old = &g_nodes[0];
  for (int i = 0; i < NODES_MAX; i++) {
    if (g_nodes[i].used && memcmp(g_nodes[i].mac, mac, 6) == 0) return &g_nodes[i];
    if (!g_nodes[i].used) { if (old->used) old = &g_nodes[i]; }
    else if (old->used && g_nodes[i].last < old->last) old = &g_nodes[i];
  }
  if (!create) return nullptr;
  Node* n = old; n->state = ""; n->name[0] = 0; n->seq = 0; n->boot = 0; n->ccx = false;
  memcpy(n->mac, mac, 6); n->used = true;
  EventBus::publish("mesh.node", "", 1);              // una scheda vicina nuova (o ricomparsa)
  return n;
}

static Node* nodeByName(const String& s) {
  for (int i = 0; i < NODES_MAX; i++) if (g_nodes[i].used && (s == g_nodes[i].name || s == macStr(g_nodes[i].mac))) return &g_nodes[i];
  return nullptr;
}

static void addMsg(const String& from, const String& text) {
  Msg m; m.from = from; m.text = text; m.when = timeNowStr();
  if (g_msgN < MSGS_MAX) g_msgs[g_msgN++] = m;
  else { for (int i = 1; i < MSGS_MAX; i++) g_msgs[i - 1] = g_msgs[i]; g_msgs[MSGS_MAX - 1] = m; }
}

// azioni ammesse da un'altra scheda: solo quelle delle Automazioni, niente riavvii a raffica
static bool cmdAllowed(const String& c) {
  String w = c; int sp = w.indexOf(' '); if (sp > 0) w = w.substring(0, sp);
  return w == "led" || w == "led-color" || w == "led-bright" || w == "gpio" || w == "note" || w == "reboot" || w == "ntp";
}

static String stateLine() {
  // nome|ip|segnale|acceso da (s)|temperatura|cpu|versione
  return cfg.hostname + "|" + netIpString() + "|" + String((int)drvWifiRssi()) + "|" + String((unsigned long)sysUptimeSec()) + "|" +
         String(sysCpuTemp(), 1) + "|" + String(sysCpuPercent()) + "|" VOS_VERSION;
}

static void handle(RxItem& it) {
  if (it.len < sizeof(MeshHdr) + M_MACLEN) { g_bad++; return; }
  MeshHdr* h = (MeshHdr*)it.data;
  size_t body = sizeof(MeshHdr) + h->len;
  if (h->ver != M_VER || h->len > M_MAXPAY || body + M_MACLEN != it.len) { g_bad++; return; }
  if (memcmp(h->src, g_me, 6) == 0) return;                       // il mio stesso messaggio ripetuto da un'altra scheda
  uint8_t mac[M_MACLEN]; sign(it.data, body, mac);
  uint8_t d = 0; for (int i = 0; i < M_MACLEN; i++) d |= mac[i] ^ it.data[body + i];
  if (d) { g_bad++; return; }                                      // firma sbagliata: chiave diversa o messaggio falso
  if (seen(h)) return;
  g_recv++;
  Node* n = nodeOf(h->src, true);
  // contro la ripetizione di vecchi messaggi: (avvio, sequenza) deve crescere
  if (n->boot && (h->boot < n->boot || (h->boot == n->boot && h->seq <= n->seq))) { if (h->boot + 100 > n->boot) return; }
  n->boot = h->boot; n->seq = h->seq; n->last = millis(); n->role = h->role; n->hops = h->hops; n->rssi = h->hops ? 0 : it.rssi;
  bool ccx = cfg.country.length() == 2 && (h->cc[0] != cfg.country[0] || h->cc[1] != cfg.country[1]);
  if (ccx && !n->ccx) auditEvent(AUD_YELLOW, "meshcc", tr("Rete schede: una scheda ha un paese diverso. Imposta lo stesso paese su tutte"));
  n->ccx = ccx;
  bool forMe = memcmp(h->dst, g_me, 6) == 0, toAll = memcmp(h->dst, BCAST, 6) == 0;
  String pay = String((const char*)(it.data + sizeof(MeshHdr)), h->len);
  if (forMe || toAll) {
    switch (h->type) {
      case T_BEACON: { int p = pay.indexOf('|'); String nm = p >= 0 ? pay.substring(p + 1) : pay; strncpy(n->name, nm.c_str(), 32); n->name[32] = 0; g_lastBeacon = millis(); break; }
      case T_STATE: {
        n->state = pay; int p = pay.indexOf('|'); String nm = p > 0 ? pay.substring(0, p) : pay; strncpy(n->name, nm.c_str(), 32); n->name[32] = 0;
        if (cfg.meshRole == 1 && mqttConnected()) {
          String f[7]; int k = 0, s = 0;
          for (int i = 0; i <= (int)pay.length() && k < 7; i++) if (i == (int)pay.length() || pay[i] == '|') { f[k++] = pay.substring(s, i); s = i + 1; }
          mqttPublishRel("mesh/" + f[0] + "/state", "{\"ip\":\"" + jsonEscape(f[1]) + "\",\"rssi\":" + (f[2].length() ? f[2] : String("0")) + ",\"uptime\":" + (f[3].length() ? f[3] : String("0")) +
                         ",\"temp\":" + (f[4].length() ? f[4] : String("0")) + ",\"cpu\":" + (f[5].length() ? f[5] : String("0")) + ",\"ver\":\"" + jsonEscape(f[6]) + "\",\"hops\":" + String(h->hops) + "}", true);
        }
        break;
      }
      case T_CMD: {
        if (!forMe) break;
        String err, res;
        if (!cmdAllowed(pay)) res = tr("azione non ammessa da un'altra scheda");
        else if (rulesAction(pay, err, "rete schede")) res = "ok";
        else res = err;
        vlog("MESH: comando da %s: %s -> %s", n->name, pay.c_str(), res.c_str());
        send(T_RESULT, h->src, pay + " -> " + res);
        break;
      }
      case T_RESULT: if (forMe) { addMsg(n->name, pay); vlog("MESH: risposta da %s: %s", n->name, pay.c_str()); } break;
      case T_TEXT: addMsg(n->name, pay); vlog("MESH: messaggio da %s: %s", n->name, pay.c_str());
        if (cfg.meshRole == 1 && mqttConnected()) mqttPublishRel("mesh/" + String(n->name) + "/text", pay);
        break;
    }
  }
  // ripetizione per chi e piu lontano (i sensori non ripetono)
  if (!forMe && h->ttl > 1 && cfg.meshRole != 2) {
    h->ttl--; h->hops++;
    vTaskDelay(pdMS_TO_TICKS(5 + (esp_random() % 20)));
    rawSend(it.data, it.len);
  }
}

static void meshTask(void*) {
  g_run = true;
  wdWatch("mesh", 30);
  uint32_t lastSt = 0;
  while (!g_stopReq) {
    wdBeat("mesh");
    RxItem it;
    if (xQueueReceive(g_q, &it, pdMS_TO_TICKS(200)) == pdTRUE) handle(it);
    uint32_t now = millis();
    if (cfg.meshRole == 1 && now - g_lastBeacon > 5000) {           // il gateway annuncia canale e nome
      g_lastBeacon = now; send(T_BEACON, BCAST, String(meshChannel()) + "|" + cfg.hostname);
    }
    if (now - lastSt > 30000UL || !lastSt) { lastSt = now ? now : 1; send(T_STATE, BCAST, stateLine()); }
    // nessun gateway sentito da 30 s, niente Wi-Fi di casa e nessuno collegato all'hotspot: prova il canale dopo
    if (cfg.meshRole != 1 && netState() == NET_AP && drvWifiApClients() == 0 && now - g_lastBeacon > 30000UL && now - g_lastHop > 30000UL) {
      g_lastHop = now;
      int ch = meshChannel() + 1; if (!regionChannelOk(ch)) ch = 1;
      netSetApChannel(ch);
    }
  }
  wdUnwatch("mesh");
  drvEspNowEnd();
  g_run = false; g_stopReq = false;
  vlog("MESH: fermata");
  vTaskDelete(NULL);
}

static void startTask() { xTaskCreatePinnedToCore(meshTask, "mesh", 6144, NULL, 2, NULL, 1); }

bool meshStart(String& err) {
  if (meshRunning()) meshStop();
  if (cfg.airOn) { err = tr("Modalita aereo attiva: radio spenta"); return false; }
  if (!drvEspNowRadioUp()) { err = tr("Il Wi-Fi non e acceso"); return false; }
  if (!keyLoad()) { err = tr("Manca la chiave comune (64 cifre esadecimali)"); return false; }
  if (!regionChannelOk(meshChannel())) { err = tr("Il canale in uso non e ammesso nel paese scelto"); return false; }
  if (!g_q) g_q = xQueueCreate(QLEN, sizeof(RxItem));
  drvEspNowMac(g_me);
  g_boot = (uint16_t)sysBootCount();
  if (!drvEspNowBegin(onRecv)) { err = tr("ESP-NOW non parte"); return false; }
  g_stopReq = false;
  startTask();
  vlog("MESH: avviata sul canale %d (ruolo %s)", meshChannel(), cfg.meshRole == 1 ? "gateway" : cfg.meshRole == 2 ? "sensore" : "nodo");
  return true;
}

static void startFromBoot() {
  String err;
  if (!meshStart(err)) vlog("MESH: non avviata (%s)", err.c_str());
}

void meshStop() {
  if (!meshRunning()) return;
  g_stopReq = true;
  for (int i = 0; i < 30 && meshRunning(); i++) vTaskDelay(pdMS_TO_TICKS(100));
}

static void stopFn() { meshStop(); }

// ---- contratto dei servizi (vos_service) ----
static int      svcState() { return meshRunning() ? SVC_ON : SVC_OFF; }
static uint32_t svcRam() { return meshRunning() ? 6144u + QLEN * (uint32_t)sizeof(RxItem) + NODES_MAX * (uint32_t)sizeof(Node) : 0u; }   // pila del task + coda + nodi
static const ServiceOps MESH_OPS = { "mesh", "ESP-NOW", "Radio", "mesh", ROLE_GUEST, ROLE_ADMIN,
                                     svcState, meshStart, meshStop, nullptr, meshJson, nullptr, svcRam };

void meshInit() {
  serviceRegister(&MESH_OPS);
  sysTaskRegister("mesh", startFromBoot, stopFn);
  if (cfg.meshAuto && cfg.meshKey.length() == 64 && !cfg.airOn) {
    // si aspetta che il Wi-Fi sia acceso (lo avvia il task di rete)
    xTaskCreatePinnedToCore([](void*) {
      for (int i = 0; i < 60 && !drvEspNowRadioUp(); i++) vTaskDelay(pdMS_TO_TICKS(500));
      vTaskDelay(pdMS_TO_TICKS(3000));
      startFromBoot();
      vTaskDelete(NULL);
    }, "meshst", 4096, NULL, 1, NULL, 1);
  }
}

static bool target(const String& to, uint8_t* mac, String& err) {
  if (to == "*" || to == "tutti" || to == "all") { memcpy(mac, BCAST, 6); return true; }
  Node* n = nodeByName(to);
  if (!n) { err = tr("Scheda non trovata nella rete"); return false; }
  memcpy(mac, n->mac, 6);
  return true;
}

bool meshSendCmd(const String& to, const String& cmd, String& err) {
  if (!meshReady()) { err = tr("Rete schede spenta"); return false; }
  if (!cmdAllowed(cmd)) { err = tr("Azione non ammessa (solo led, led-color, led-bright, gpio, note, reboot, ntp sync)"); return false; }
  uint8_t mac[6];
  if (to == "*" || !target(to, mac, err)) { if (!err.length()) err = tr("Indica una scheda (i comandi vanno a una sola scheda)"); return false; }
  return send(T_CMD, mac, cmd);
}

bool meshSendText(const String& to, const String& text, String& err) {
  if (!meshReady()) { err = tr("Rete schede spenta"); return false; }
  uint8_t mac[6];
  if (!target(to, mac, err)) return false;
  String t = cleanAscii(text); t.replace("\n", " ");
  if (!t.length() || t.length() > 150) { err = tr("Messaggio vuoto o troppo lungo (max 150)"); return false; }
  addMsg(cfg.hostname + " -> " + to, t);
  return send(T_TEXT, mac, t);
}

int meshNodeCount() { int n = 0; for (int i = 0; i < NODES_MAX; i++) if (g_nodes[i].used) n++; return n; }

String meshJson() {
  String j = "{\"run\":" + String(meshReady() ? "true" : "false") + ",\"auto\":" + String(cfg.meshAuto ? "true" : "false") +
             ",\"role\":" + String(cfg.meshRole) + ",\"hasKey\":" + String(cfg.meshKey.length() == 64 ? "true" : "false") +
             ",\"ch\":" + String(meshChannel()) + ",\"chMax\":" + String(regionChannels()) + ",\"me\":\"" + netMac(drvEspNowIsAp()) +
             "\",\"sent\":" + String((unsigned long)g_sent) + ",\"recv\":" + String((unsigned long)g_recv) + ",\"bad\":" + String((unsigned long)g_bad) + ",\"nodes\":[";
  bool first = true;
  for (int i = 0; i < NODES_MAX; i++) {
    Node& n = g_nodes[i];
    if (!n.used) continue;
    if (!first) j += ","; first = false;
    j += "{\"mac\":\"" + macStr(n.mac) + "\",\"name\":\"" + jsonEscape(String(n.name)) + "\",\"role\":" + String(n.role) + ",\"hops\":" + String(n.hops) +
         ",\"rssi\":" + String(n.rssi) + ",\"ago\":" + String((unsigned long)((millis() - n.last) / 1000)) + ",\"ccx\":" + String(n.ccx ? "true" : "false") +
         ",\"state\":\"" + jsonEscape(n.state) + "\"}";
  }
  j += "],\"msgs\":[";
  for (int i = g_msgN - 1; i >= 0; i--) {
    j += "{\"from\":\"" + jsonEscape(g_msgs[i].from) + "\",\"text\":\"" + jsonEscape(g_msgs[i].text) + "\",\"when\":\"" + jsonEscape(g_msgs[i].when) + "\"}";
    if (i) j += ",";
  }
  return j + "]}";
}

String meshText() {
  String t = trf("Rete schede: %s, canale %d, ruolo %s", meshReady() ? tr("accesa") : tr("spenta"), meshChannel(),
                 cfg.meshRole == 1 ? "gateway" : cfg.meshRole == 2 ? tr("sensore") : tr("nodo")) + "\n";
  for (int i = 0; i < NODES_MAX; i++) {
    Node& n = g_nodes[i];
    if (!n.used) continue;
    t += String("  ") + n.name + "  " + macStr(n.mac) + trf("  %d salti  %lu s fa", n.hops, (unsigned long)((millis() - n.last) / 1000)) + (n.ccx ? String("  ") + tr("(paese diverso!)") : String("")) + "\n";
  }
  return t;
}
#endif  // VOS_WITH_MESH
