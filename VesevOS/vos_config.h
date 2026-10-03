// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// VesevOS - vos_config.h
// Configurazione unica in stile OpenWrt, salvata in /flash/vesevos.conf
#pragma once
#include <Arduino.h>
#include "vos_common.h"

struct VosConfig {
  String hostname, domain;
  String apSsid, apPass;
  bool   staEnabled;
  String staSsid, staPass;
  bool   staDhcp;
  String ip, mask, gw, dns1, dns2;
  String authSalt, authHash;     // password (mai in chiaro)
  uint8_t  ledMode;
  uint32_t ledColor;             // 0xRRGGBB
  uint8_t  ledBrightness;        // 0..255
  uint8_t  ledPin;
  uint8_t  led2Mode;             // LED aggiuntivo (rosso): 0 spento 1 acceso 2 battito
  uint8_t  led2Pin;              // pin del LED aggiuntivo
  bool     led2Invert;           // true = si accende con livello basso
  uint8_t  led2Bright;           // 0..255
  bool   ntpOn, ntpServe;        // client NTP, server NTP per altri dispositivi
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
