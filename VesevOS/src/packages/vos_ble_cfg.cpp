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
#include "../core/vos_parse.h"

// Il comando si legge con viste sul testo ricevuto (nessuna copia). La risposta va in reply (buffer fisso, max 160 caratteri).
// La configurazione tiene ancora testi Arduino: si convertono solo i valori che ci finiscono dentro.

void bleCfgRun(const char* cmd, bool& stopAfter, ErrBuf& reply) {
  stopAfter = false;
  std::string_view c = trimView(cmd ? cmd : "");
  size_t sp = c.find(' ');
  std::string_view w = c, rest;
  if (sp != std::string_view::npos && sp > 0) { w = c.substr(0, sp); rest = trimView(c.substr(sp + 1)); }
  cfgSetOrigin("bluetooth");
  if (w == "status") {
    char ip[16]; netIpTo(ip, sizeof ip);
    reply.setf("%s %s %s", cfg.hostname.c_str(), ip, cfg.country.length() ? cfg.country.c_str() : "--");
    return;
  }
  if (w == "done") { stopAfter = true; reply = "ok"; return; }
  if (w == "wifi") {
    size_t p = rest.find('|');
    std::string_view ssid = p != std::string_view::npos ? rest.substr(0, p) : rest;
    std::string_view pass = p != std::string_view::npos ? rest.substr(p + 1) : std::string_view();
    if (ssid.empty() || ssid.size() > 32 || (!pass.empty() && (pass.size() < 8 || pass.size() > 63))) { reply = tr("rete o password non valide"); return; }
    cfg.staSsid = ssid; cfg.staPass = pass; cfg.staEnabled = true; cfg.staDhcp = true;
    bool ntpBack = cfgNtpAfterWifi();
    cfgSave(); netReconfigure(); if (ntpBack) timeApply();
    reply = "ok";
    return;
  }
  if (w == "country") {
    char cc[8];                                   // il codice paese ha 2 lettere: oltre la capacita non puo essere valido
    size_t n = rest.size() < sizeof(cc) - 1 ? rest.size() : sizeof(cc) - 1;
    for (size_t i = 0; i < n; i++) cc[i] = (char)toupper((unsigned char)rest[i]);
    cc[n] = 0;
    if (n != rest.size() || !regionValid(cc)) { reply = tr("paese sconosciuto"); return; }
    cfg.country = cc; cfgSave(); netReconfigure();
    reply = "ok";
    return;
  }
  if (w == "name") {
    char nm[40];                                  // hostnameValid accetta al massimo 32 caratteri
    size_t n = rest.size() < sizeof(nm) - 1 ? rest.size() : sizeof(nm) - 1;
    for (size_t i = 0; i < n; i++) nm[i] = (char)tolower((unsigned char)rest[i]);
    nm[n] = 0;
    if (n != rest.size() || !hostnameValid(std::string_view(nm, n))) { reply = tr("nome non valido"); return; }
    cfg.hostname = nm; cfgSave(); netReconfigure();
    reply = "ok";
    return;
  }
  if (w == "mesh") {
    size_t p = rest.find(' ');
    long role = p != std::string_view::npos && p > 0 ? viewToLong(rest.substr(0, p)) : -1;
    std::string_view key = p != std::string_view::npos && p > 0 ? rest.substr(p + 1) : std::string_view();
    if (role < 0 || role > 2 || key.size() != 64) { reply = tr("uso: mesh <0|1|2> <chiave di 64 cifre>"); return; }
    char k[65];
    for (size_t i = 0; i < 64; i++) k[i] = (char)tolower((unsigned char)key[i]);
    k[64] = 0;
    cfg.meshRole = role; cfg.meshKey = k; cfgSave();
    reply = "ok";
    return;
  }
  reply = tr("comando sconosciuto (wifi, country, name, mesh, status, done)");
}
#endif
