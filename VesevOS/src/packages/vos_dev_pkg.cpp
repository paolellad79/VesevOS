// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_dev_pkg.cpp
// Periferiche VIRTUALI dei package (MQTT, ESP-NOW, Bluetooth, MFA, statistiche) per il registro delle periferiche.
// Stanno qui e non in devices/: il Device Manager non conosce i package, sono loro a iscriversi (devPkgRegister).
#include "vos_dev_pkg.h"
#include "../devices/vos_dev.h"
#include "../core/vos_common.h"
#include "../core/vos_config.h"
#include "../core/vos_i18n.h"
#include "../core/vos_service.h"
#include "vos_mqtt.h"
#include "vos_mesh.h"
#include "vos_ble.h"
#include "vos_mfa.h"
#include "vos_stats.h"

#if VOS_WITH_MQTT
static bool js_mqtt(HeapBuf& out) { return mqttStatusJson(out); }
static int  st_mqtt() { return serviceState("mqtt"); }
static bool set_mqtt(bool on, ErrBuf& err) { if (!on) return serviceStop("mqtt", err); return serviceStart("mqtt", err); }
static bool act_mqtt(const char* a, const char* arg, int role, HeapBuf& out, ErrBuf& err) {
  if (!strcmp(a, "test")) {
    if (role < ROLE_OPER) { err = tr("Il tuo ruolo non permette questo comando"); return false; }
    if (!mqttPublishRel("test", "VesevOS " VOS_VERSION)) { err = tr("MQTT non collegato"); return false; }
    return out.add("{\"ok\":true}");
  }
  err = tr("Azione non valida"); return false;
}
#endif
#if VOS_WITH_MESH
static bool js_mesh(HeapBuf& out) { return meshJson(out); }
static int  st_mesh() { return serviceState("mesh"); }
static bool set_mesh(bool on, ErrBuf& err) { if (!on) return serviceStop("mesh", err); return serviceStart("mesh", err); }
#endif
#if VOS_WITH_BLE
static bool js_ble(HeapBuf& out) { return bleJson(out); }
static int  st_ble() { return serviceState("ble"); }
static bool set_ble(bool on, ErrBuf& err) { if (!on) { bleUserOff(); return true; } return bleUserOn(0, err); }
#endif
#if VOS_WITH_MFA
static int  st_mfa() { for (int i = 0; i < VOS_MAX_USERS; i++) if (cfg.users[i].name.length() && mfaOn(i)) return 1; return 0; }
static bool js_mfa(HeapBuf& out) {
  int n = 0, t = 0;
  for (int i = 0; i < VOS_MAX_USERS; i++) if (cfg.users[i].name.length()) { t++; if (mfaOn(i)) n++; }
  return out.addf("{\"users\":%d,\"on\":%d}", t, n);
}
#endif
#if VOS_WITH_STATS
static bool js_stat(HeapBuf& out) { return statsJson(out); }
static int  st_stat() { return cfg.statOn ? 1 : 0; }
static bool set_stat(bool on, ErrBuf& err) {
  if (on && !cfg.statOn) statsReset();
  cfg.statOn = on; cfgSetOrigin("periferiche"); cfgSave(); return true;
}
#endif

// stesso ordine di prima: in testa all'elenco, prima dell'hardware. L'ultima riga vuota tiene valida la tabella anche senza package.
static const DevOps VIRT[] = {
#if VOS_WITH_MQTT
  { "mqtt", "MQTT",              "Messaggi",   DEV_VIRT, "mqtt", ROLE_OPER,  st_mqtt, set_mqtt, js_mqtt, act_mqtt },
#endif
#if VOS_WITH_MESH
  { "mesh", "ESP-NOW",           "Radio",      DEV_VIRT, "mesh", ROLE_GUEST, st_mesh, set_mesh, js_mesh,       nullptr },
#endif
#if VOS_WITH_BLE
  { "ble",  "Bluetooth",         "Radio",      DEV_VIRT, "ble",  ROLE_ADMIN, st_ble,  set_ble,  js_ble,        nullptr },
#endif
#if VOS_WITH_MFA
  { "mfa",  "MFA (due passaggi)", "Sicurezza", DEV_VIRT, "mfa",  ROLE_GUEST, st_mfa,  nullptr,  js_mfa,         nullptr },
#endif
#if VOS_WITH_STATS
  { "stat", "Statistiche",       "Misure",     DEV_VIRT, "stat", ROLE_OPER,  st_stat, set_stat, js_stat,       nullptr },
#endif
  { nullptr, nullptr, nullptr, DEV_VIRT, nullptr, 0, nullptr, nullptr, nullptr, nullptr },
};

void devPkgRegister() { devSetVirtual(VIRT, (int)(sizeof(VIRT) / sizeof(VIRT[0])) - 1); }
