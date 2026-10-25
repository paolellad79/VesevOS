// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_mesh.cpp
#include <math.h>
#include "../drivers/vos_drv_task.h"
#include "vos_mesh.h"
#include "vos_mesh_msg.h"
#include "../core/vos_eventbus.h"
#include "../core/vos_config.h"
#include "../core/vos_region.h"
#include "../net/vos_net.h"
#include "../core/vos_sys.h"
#include "../core/vos_log.h"
#include "../core/vos_i18n.h"
#include "../core/vos_util.h"
#include "../core/vos_lock.h"
#include "vos_rules.h"
#include "vos_mqtt.h"
#include "../core/vos_audit.h"
#include "../core/vos_wd.h"
#include "../core/vos_time.h"
#include "../drivers/vos_drv_wifi.h"
#include "../drivers/vos_drv_espnow.h"
#include "../drivers/vos_drv_heap.h"
#include "../core/vos_service.h"
#include "mbedtls/md.h"
#include "../drivers/vos_drv_time.h"
#include "../drivers/vos_drv_sys.h"

#if VOS_WITH_MESH
#define M_VER     1
#define M_TTL     3
#define M_MAXPAY  MESH_MAXPAY
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

struct RxItem { uint8_t len; int8_t rssi; uint8_t data[sizeof(MeshHdr) + M_MAXPAY + M_MACLEN]; };

// Nodi e messaggi stanno in UN blocco fisso, creato alla prima accensione del servizio (spento di fabbrica: prima non costa nulla)
// e mai liberato: il web server puo leggerlo anche mentre il servizio si ferma. Va in PSRAM se c'e. I campi di lavoro (pay, out, json)
// li usa solo il task mesh, cosi non servono buffer grandi sullo stack.
struct Tables {
  MeshNode nodes[NODES_MAX];
  MeshMsg  msgs[MSGS_MAX]; int msgN;
  char     pay[M_MAXPAY + 1];   // testo del messaggio in arrivo, chiuso con zero
  char     out[200];            // testo da mandare, nome del tema MQTT
  char     json[640];           // stato di un nodo per MQTT
};

static Tables* g_t = nullptr;
static VosMutex g_mx;             // protegge nodes e msgs tra il task mesh e chi legge (web, shell)
static uint32_t g_seen[SEEN_MAX]; static int g_seenI = 0;
static QueueHandle_t g_q = nullptr;
static volatile bool g_run = false, g_stopReq = false;
static uint8_t g_me[6];
static uint32_t g_seq = 0;
static uint16_t g_boot = 0;
static uint8_t g_key[32];
static uint32_t g_lastBeacon = 0, g_lastHop = 0;
static uint32_t g_sent = 0, g_recv = 0, g_bad = 0;
static const uint8_t BCAST[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

static bool meshReady() { return g_run; }
bool meshRunning() { return sysTaskRunning("mesh"); }

void meshNewKey(char (&k)[MESH_KEY_CHARS + 1]) {
  static const char* hx = "0123456789abcdef";
  for (int i = 0; i < 32; i++) { uint8_t b = drvRandom32() & 0xFF; k[i * 2] = hx[b >> 4]; k[i * 2 + 1] = hx[b & 15]; }
  k[MESH_KEY_CHARS] = 0;
}

static bool keyLoad() { return meshKeyParse(std::string_view(cfg.meshKey.c_str(), cfg.meshKey.length()), g_key); }

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

// Manda un messaggio di len byte (al massimo M_MAXPAY: il resto si taglia).
static bool send(uint8_t type, const uint8_t* dst, const char* data, size_t len) {
  if (!meshReady()) return false;
  uint8_t buf[sizeof(MeshHdr) + M_MAXPAY + M_MACLEN];
  MeshHdr* h = (MeshHdr*)buf;
  size_t pl = len > M_MAXPAY ? M_MAXPAY : len;
  h->m0 = 'V'; h->m1 = 'M'; h->ver = M_VER; h->type = type; h->ttl = M_TTL; h->hops = 0; h->role = cfg.meshRole;
  h->cc[0] = cfg.country.length() == 2 ? cfg.country[0] : '0'; h->cc[1] = cfg.country.length() == 2 ? cfg.country[1] : '1';
  memcpy(h->src, g_me, 6); memcpy(h->dst, dst, 6);
  h->boot = g_boot; h->seq = ++g_seq; h->len = pl;
  memcpy(buf + sizeof(MeshHdr), data, pl);
  sign(buf, sizeof(MeshHdr) + pl, buf + sizeof(MeshHdr) + pl);
  rawSend(buf, sizeof(MeshHdr) + pl + M_MACLEN);
  return true;
}
static bool sendStr(uint8_t type, const uint8_t* dst, const char* s) { return send(type, dst, s, strlen(s)); }

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

// Il blocco g_mx deve essere preso dal chiamante. created = e una scheda nuova (o ricomparsa): l'evento si manda fuori dal blocco.
static MeshNode* nodeOf(const uint8_t* mac, bool create, bool& created) {
  Tables& T = *g_t;
  MeshNode* old = &T.nodes[0];
  created = false;
  for (int i = 0; i < NODES_MAX; i++) {
    if (T.nodes[i].used && memcmp(T.nodes[i].mac, mac, 6) == 0) return &T.nodes[i];
    if (!T.nodes[i].used) { if (old->used) old = &T.nodes[i]; }
    else if (old->used && T.nodes[i].last < old->last) old = &T.nodes[i];
  }
  if (!create) return nullptr;
  MeshNode* n = old; n->state[0] = 0; n->name[0] = 0; n->seq = 0; n->boot = 0; n->ccx = false;
  memcpy(n->mac, mac, 6); n->used = true;
  created = true;
  return n;
}

// Cerca una scheda per nome o per MAC e ne copia il MAC.
static bool nodeMacByName(const char* s, uint8_t* mac) {
  if (!g_t) return false;
  VosLock L(g_mx);
  for (int i = 0; i < NODES_MAX; i++) {
    const MeshNode& n = g_t->nodes[i];
    if (!n.used) continue;
    char ms[18]; meshMacStr(n.mac, ms);
    if (strcmp(s, n.name) == 0 || strcmp(s, ms) == 0) { memcpy(mac, n.mac, 6); return true; }
  }
  return false;
}

static void addMsg(const char* from, const char* text) {
  if (!g_t) return;
  VosLock L(g_mx);
  Tables& T = *g_t;
  MeshMsg* m;
  if (T.msgN < MSGS_MAX) m = &T.msgs[T.msgN++];
  else { memmove(&T.msgs[0], &T.msgs[1], (MSGS_MAX - 1) * sizeof(MeshMsg)); m = &T.msgs[MSGS_MAX - 1]; }
  meshCopy(m->from, sizeof m->from, from);
  meshCopy(m->text, sizeof m->text, text);
  timeNowFmt(m->when, sizeof m->when);
}

// nome|ip|segnale|acceso da (s)|temperatura|cpu|versione
static size_t stateLine(char* out, size_t cap) {
  StrBuf sb(out, cap);
  char ip[16]; netIpTo(ip, sizeof ip);
  sb.addf("%s|%s|%d|%lu|%ld|%d|%s", cfg.hostname.c_str(), ip, (int)drvWifiRssi(), (unsigned long)sysUptimeSec(),
          (long)lroundf(sysCpuTemp()), sysCpuPercent(), VOS_VERSION);
  return sb.length();
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
  Tables& T = *g_t;
  // dopo il testo nel pacchetto c'e la firma, non uno zero: si copia in un buffer chiuso con zero (la vista non lo richiede, le funzioni C si)
  memcpy(T.pay, it.data + sizeof(MeshHdr), h->len); T.pay[h->len] = 0;
  const std::string_view pay(T.pay);
  bool forMe = memcmp(h->dst, g_me, 6) == 0, toAll = memcmp(h->dst, BCAST, 6) == 0;
  bool created = false, replay = false, ccNew = false;
  char nm[MESH_NAME_CAP];
  {
    VosLock L(g_mx);
    MeshNode* n = nodeOf(h->src, true, created);
    // contro la ripetizione di vecchi messaggi: (avvio, sequenza) deve crescere
    if (n->boot && (h->boot < n->boot || (h->boot == n->boot && h->seq <= n->seq))) { if (h->boot + 100 > n->boot) replay = true; }
    if (!replay) {
      n->boot = h->boot; n->seq = h->seq; n->last = drvMillis(); n->role = h->role; n->hops = h->hops; n->rssi = h->hops ? 0 : it.rssi;
      bool ccx = cfg.country.length() == 2 && (h->cc[0] != cfg.country[0] || h->cc[1] != cfg.country[1]);
      ccNew = ccx && !n->ccx;
      n->ccx = ccx;
      if (forMe || toAll) {
        if (h->type == T_BEACON) meshCopy(n->name, sizeof n->name, meshBeaconName(pay));
        else if (h->type == T_STATE) { meshCopy(n->state, sizeof n->state, pay); meshCopy(n->name, sizeof n->name, meshStateName(pay)); }
      }
      meshCopy(nm, sizeof nm, n->name);
    }
  }
  if (created) EventBus::publish("mesh.node", "", 1);              // una scheda vicina nuova (o ricomparsa)
  if (replay) return;
  if (ccNew) auditEvent(AUD_YELLOW, "meshcc", tr("Rete schede: una scheda ha un paese diverso. Imposta lo stesso paese su tutte"));
  if (forMe || toAll) {
    switch (h->type) {
      case T_BEACON: g_lastBeacon = drvMillis(); break;
      case T_STATE:
        if (cfg.meshRole == 1 && mqttConnected()) {
          std::string_view f[MESH_FIELDS]; meshSplitState(pay, f);
          StrBuf jb(T.json, sizeof T.json); meshStateJson(jb, f, h->hops);
          StrBuf sub(T.out, sizeof T.out); sub.add("mesh/"); sub.addn(f[0].data(), f[0].size()); sub.add("/state");
          if (!jb.truncated() && !sub.truncated()) mqttPublishRel(T.out, T.json, true);   // JSON troncato: non si pubblica
        }
        break;
      case T_CMD: {
        if (!forMe) break;
        ErrBuf err;
        const char* res;
        if (!meshCmdAllowed(pay)) res = tr("azione non ammessa da un'altra scheda");
        else if (rulesAction(T.pay, err, "rete schede")) res = "ok";
        else res = err.c_str();
        vlog("MESH: comando da %s: %s -> %s", nm, T.pay, res);
        StrBuf out(T.out, sizeof T.out); out.add(T.pay); out.add(" -> "); out.add(res);
        send(T_RESULT, h->src, T.out, out.length());
        break;
      }
      case T_RESULT: if (forMe) { addMsg(nm, T.pay); vlog("MESH: risposta da %s: %s", nm, T.pay); } break;
      case T_TEXT: addMsg(nm, T.pay); vlog("MESH: messaggio da %s: %s", nm, T.pay);
        if (cfg.meshRole == 1 && mqttConnected()) { snprintf(T.out, sizeof T.out, "mesh/%s/text", nm); mqttPublishRel(T.out, T.pay); }
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
  wdWatch("mesh", wdLimit("mesh"));                   // base 90 s
  uint32_t lastSt = 0;
  Tables& T = *g_t;
  while (!g_stopReq) {
    wdBeat("mesh");
    RxItem it;
    if (xQueueReceive(g_q, &it, pdMS_TO_TICKS(200)) == pdTRUE) handle(it);
    uint32_t now = drvMillis();
    if (cfg.meshRole == 1 && now - g_lastBeacon > 5000) {           // il gateway annuncia canale e nome
      g_lastBeacon = now;
      snprintf(T.out, sizeof T.out, "%d|%s", meshChannel(), cfg.hostname.c_str());
      sendStr(T_BEACON, BCAST, T.out);
    }
    if (now - lastSt > 30000UL || !lastSt) { lastSt = now ? now : 1; size_t n = stateLine(T.out, sizeof T.out); send(T_STATE, BCAST, T.out, n); }
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

static void startTask() {
  drvTaskCreate(meshTask, "mesh", 6144, NULL, 2, NULL, DRV_CORE_ANY);   // core: automatico (prima: core 1 fisso)
}

bool meshStart(ErrBuf& err) {
  if (meshRunning()) meshStop();
  if (cfg.airOn) { err = tr("Modalita aereo attiva: radio spenta"); return false; }
  if (!drvEspNowRadioUp()) { err = tr("Il Wi-Fi non e acceso"); return false; }
  if (!keyLoad()) { err = tr("Manca la chiave comune (64 cifre esadecimali)"); return false; }
  if (!regionChannelOk(meshChannel())) { err = tr("Il canale in uso non e ammesso nel paese scelto"); return false; }
  g_mx.init();
  if (!g_t) g_t = (Tables*)drvCallocBig(sizeof(Tables));
  if (!g_t) { err = tr("Memoria esaurita"); return false; }
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
  ErrBuf err;
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
static uint32_t svcRam() { return meshRunning() ? 6144u + QLEN * (uint32_t)sizeof(RxItem) : 0u; }   // pila del task + coda (nodi e messaggi stanno in PSRAM, se c'e)
static const ServiceOps MESH_OPS = { "mesh", "ESP-NOW", "Radio", "mesh", ROLE_GUEST, ROLE_ADMIN,
                                     svcState, meshStart, meshStop, nullptr, meshJson, nullptr, svcRam };

void meshInit() {
  g_mx.init();
  serviceRegister(&MESH_OPS);
  sysTaskRegister("mesh", startFromBoot, stopFn);
  if (cfg.meshAuto && cfg.meshKey.length() == 64 && !cfg.airOn) {
    // si aspetta che il Wi-Fi sia acceso (lo avvia il task di rete)
    drvTaskCreate([](void*) {
      for (int i = 0; i < 60 && !drvEspNowRadioUp(); i++) vTaskDelay(pdMS_TO_TICKS(500));
      vTaskDelay(pdMS_TO_TICKS(3000));
      startFromBoot();
      vTaskDelete(NULL);
    }, "meshst", 4096, NULL, 1, NULL, DRV_CORE_ANY);   // core: automatico (prima: core 1 fisso)
  }
}

static bool target(const char* to, uint8_t* mac, ErrBuf& err) {
  if (!strcmp(to, "*") || !strcmp(to, "tutti") || !strcmp(to, "all")) { memcpy(mac, BCAST, 6); return true; }
  if (!nodeMacByName(to, mac)) { err = tr("Scheda non trovata nella rete"); return false; }
  return true;
}

bool meshSendCmd(const char* to, const char* cmd, ErrBuf& err) {
  if (!meshReady()) { err = tr("Rete schede spenta"); return false; }
  if (!meshCmdAllowed(cmd)) { err = tr("Azione non ammessa (solo led, led-color, led-bright, gpio, note, reboot, ntp sync)"); return false; }
  uint8_t mac[6];
  if (!strcmp(to, "*") || !target(to, mac, err)) { if (err.empty()) err = tr("Indica una scheda (i comandi vanno a una sola scheda)"); return false; }
  return send(T_CMD, mac, cmd, strlen(cmd));
}

bool meshSendText(const char* to, const char* text, ErrBuf& err) {
  if (!meshReady()) { err = tr("Rete schede spenta"); return false; }
  uint8_t mac[6];
  if (!target(to, mac, err)) return false;
  std::string t; t.reserve(152);
  size_t n = meshCleanText(text, t, 151);
  if (n == 0 || n > 150) { err = tr("Messaggio vuoto o troppo lungo (max 150)"); return false; }
  char from[MESH_FROM_CAP]; snprintf(from, sizeof from, "%s -> %s", cfg.hostname.c_str(), to);
  addMsg(from, t.c_str());
  return send(T_TEXT, mac, t.data(), t.size());
}

int meshNodeCount() {
  int n = 0;
  if (g_t) for (int i = 0; i < NODES_MAX; i++) if (g_t->nodes[i].used) n++;
  return n;
}

std::string meshJsonStd() {
  char me[20]; netMacTo(drvEspNowIsAp(), me, sizeof me);
  std::string j = meshBuild([&](StrBuf& sb) {
    VosLock L(g_mx);
    MeshJsonIn in = { meshReady(), cfg.meshAuto, cfg.meshKey.length() == 64, cfg.meshRole, meshChannel(), regionChannels(), me,
                      g_sent, g_recv, g_bad, (uint32_t)drvMillis(), g_t ? g_t->nodes : nullptr, g_t ? NODES_MAX : 0, g_t ? g_t->msgs : nullptr, g_t ? g_t->msgN : 0 };
    meshJsonBuild(sb, in);
  });
  return j.empty() ? std::string("{\"error\":\"busy\"}") : j;   // non entrava nel buffer (i dati cambiavano): mai JSON a meta
}

bool meshJson(HeapBuf& out) { std::string j = meshJsonStd(); return out.add(j.data(), j.size()); }

std::string meshText() {
  return meshBuild([&](StrBuf& sb) {
    sb.addf(tr("Rete schede: %s, canale %d, ruolo %s"), meshReady() ? tr("accesa") : tr("spenta"), meshChannel(),
            cfg.meshRole == 1 ? "gateway" : cfg.meshRole == 2 ? tr("sensore") : tr("nodo"));
    sb.add('\n');
    if (!g_t) return;
    VosLock L(g_mx);
    uint32_t now = drvMillis();
    for (int i = 0; i < NODES_MAX; i++) {
      const MeshNode& n = g_t->nodes[i];
      if (!n.used) continue;
      char mac[18]; meshMacStr(n.mac, mac);
      sb.add("  "); sb.add(n.name); sb.add("  "); sb.add(mac);
      sb.addf(tr("  %d salti  %lu s fa"), (int)n.hops, (unsigned long)((now - n.last) / 1000));
      if (n.ccx) { sb.add("  "); sb.add(tr("(paese diverso!)")); }
      sb.add('\n');
    }
  });
}
#endif  // VOS_WITH_MESH
