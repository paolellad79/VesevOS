// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_ble_cfg.cpp (strato 4: comandi di prima configurazione dal telefono)
#include "vos_ble_cfg.h"

#if VOS_WITH_BLE
#include "../core/vos_config.h"
#include "../core/vos_region.h"
#include "../net/vos_net.h"
#include "../core/vos_i18n.h"
#include "../core/vos_util.h"
#include "../core/vos_time.h"

String bleCfgRun(String c, bool& stopAfter) {
  stopAfter = false;
  c.trim();
  String w = c, rest; int sp = c.indexOf(' ');
  if (sp > 0) { w = c.substring(0, sp); rest = c.substring(sp + 1); rest.trim(); }
  cfgSetOrigin("bluetooth");
  if (w == "status") return cfg.hostname + " " + netIpString() + " " + (cfg.country.length() ? cfg.country : String("--"));
  if (w == "done") { stopAfter = true; return "ok"; }
  if (w == "wifi") {
    int p = rest.indexOf('|');
    String ssid = p >= 0 ? rest.substring(0, p) : rest, pass = p >= 0 ? rest.substring(p + 1) : String("");
    if (!ssid.length() || ssid.length() > 32 || (pass.length() && (pass.length() < 8 || pass.length() > 63))) return tr("rete o password non valide");
    cfg.staSsid = ssid; cfg.staPass = pass; cfg.staEnabled = true; cfg.staDhcp = true;
    bool ntpBack = cfgNtpAfterWifi();
    cfgSave(); netReconfigure(); if (ntpBack) timeApply();
    return "ok";
  }
  if (w == "country") {
    rest.toUpperCase();
    if (!regionValid(rest)) return tr("paese sconosciuto");
    cfg.country = rest; cfgSave(); netReconfigure();
    return "ok";
  }
  if (w == "name") {
    rest.toLowerCase();
    if (!hostnameValid(rest)) return tr("nome non valido");
    cfg.hostname = rest; cfgSave(); netReconfigure();
    return "ok";
  }
  if (w == "mesh") {
    int p = rest.indexOf(' ');
    int role = p > 0 ? rest.substring(0, p).toInt() : -1;
    String key = p > 0 ? rest.substring(p + 1) : String("");
    key.toLowerCase();
    if (role < 0 || role > 2 || key.length() != 64) return tr("uso: mesh <0|1|2> <chiave di 64 cifre>");
    cfg.meshRole = role; cfg.meshKey = key; cfgSave();
    return "ok";
  }
  return tr("comando sconosciuto (wifi, country, name, mesh, status, done)");
}
#endif
