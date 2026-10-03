// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_config.h
// Configurazione unica in stile OpenWrt, salvata in /flash/vesevos.conf
#pragma once
#include <Arduino.h>
#include "vos_common.h"

struct VosConfig {
  String hostname, domain;
  String apSsid, apPass;
  bool   staEnabled;
  bool     mqttAuto;             // MQTT: parte da solo all'avvio
  String   mqttHost, mqttUser, mqttPass, mqttPrefix;
  uint16_t mqttPort;
  uint16_t mqttEvery;            // secondi tra due invii dello stato
  bool     mqttHa;               // Home Assistant: presentazione automatica
  uint8_t  banFails;             // password sbagliate prima del blocco (3-20)
  uint32_t banSecs;              // durata del primo blocco in secondi (poi raddoppia, max 1 ora)
  uint8_t  airOn;                // modalita aereo: 1 = radio spenta
  uint8_t  airExit;              // come si riattiva: 0 al prossimo avvio, 1 dopo un tempo, 2 a un orario, 3 solo a mano
  uint32_t airUntil;             // airExit 1: epoch di fine (se l'ora c'era) oppure secondi dall'avvio
  uint16_t airAt;                // airExit 2: minuti dalla mezzanotte (HH*60+MM)
  String staSsid, staPass;
  bool   staDhcp;
  String ip, mask, gw, dns1, dns2;
  String authSalt, authHash;     // password (mai in chiaro)
  bool   serialAuth;             // true = la shell seriale chiede la password (predefinito)
  uint8_t  ledMode;
  uint32_t ledColor;             // 0xRRGGBB
  uint8_t  ledBrightness;        // 0..255
  uint8_t  ledPin;
  bool   ntpOn, ntpServe;        // client NTP, server NTP per altri dispositivi
  uint32_t ntpEvery;             // minuti tra due sincronizzazioni NTP (0 = solo all'avvio)
  String ntpServer, tz, tzName;  // server, fuso (formato POSIX), nome
  uint8_t dateFmt, timeFmt, tempUnit;   // data: 0 GG/MM/AAAA 1 AAAA-MM-GG 2 MM/GG/AAAA; ora: 0=24h 1=12h; temp: 0=C 1=F
  uint16_t cpuMhz;               // 0 = automatico, altrimenti 80/160/240
  String lang;                   // codice lingua: "it" (predefinita) o file /lang/<codice>.json
  bool   sdEnabled;
  uint8_t sdCs, sdSck, sdMiso, sdMosi;
};

extern VosConfig cfg;

void   cfgDefaults();
bool   cfgLoad();                       // da file (true se trovato)
bool   cfgSave();                       // salvataggio sicuro (.tmp/.bak)
String cfgExport(bool withSecrets);     // testo OpenWrt
bool   cfgImport(const String& text, String& err);   // ripristino: chiavi assenti restano
void   cfgFactoryReset();
