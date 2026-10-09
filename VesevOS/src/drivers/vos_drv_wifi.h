// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_drv_wifi.h
// STRATO 1 (driver): unico file che usa la libreria Wi-Fi di Arduino-ESP32 (WiFi.*) per la RETE della scheda:
// modo (spento / client / hotspot / entrambi), collegamento alla rete di casa, hotspot, IP statico, scansione e informazioni (segnale, canale...).
// Non decide quando collegarsi ne cosa fare se cade: lo decide il servizio di rete (vos_net).
// Gli indirizzi IP sono numeri con il primo numero in alto (192.168.1.5 = 0xC0A80105), come ipParse()/ipToStr() di vos_util.
#pragma once
#include <Arduino.h>

enum DrvWifiMode : uint8_t { DW_OFF = 0, DW_STA = 1, DW_AP = 2, DW_AP_STA = 3 };
enum DrvWifiSta  : uint8_t { DWS_IDLE = 0, DWS_NO_SSID = 1, DWS_CONNECTED = 2, DWS_OTHER = 3 };

// modo e spegnimento
DrvWifiMode drvWifiMode();
void        drvWifiSetMode(DrvWifiMode m);
void        drvWifiDisconnect(bool wifiOff, bool eraseAp);         // scollega dalla rete di casa (wifiOff = spegne anche la radio)

// client (rete di casa)
void        drvWifiSetHostname(const char* hostname);
void        drvWifiStaConfigStatic(uint32_t ip, uint32_t gw, uint32_t mask, uint32_t dns1, uint32_t dns2);
void        drvWifiStaConfigDhcp();
void        drvWifiStaBegin(const char* hostname, const char* ssid, const char* pass);
DrvWifiSta  drvWifiStaStatus();
String      drvWifiSsid();
int         drvWifiRssi();
uint32_t    drvWifiLocalIp();
uint32_t    drvWifiGateway();
uint32_t    drvWifiMask();
uint32_t    drvWifiDns(int i);

// hotspot
bool        drvWifiApStart(const char* ssid, const char* pass, int channel, uint32_t ip, uint32_t gw, uint32_t mask);   // imposta l'indirizzo e accende
bool        drvWifiApRetune(const char* ssid, const char* pass, int channel);                                           // cambia canale senza spegnere
uint32_t    drvWifiApIp();
int         drvWifiApClients();

// radio: paese, potenza, risparmio energia
// Imposta il paese (cc "01" = prudente), limita i canali se maxCh e' 11 (o meno) e la potenza (dBm). Ritorna il paese davvero applicato ("" = radio spenta).
String      drvWifiApplyRegion(const char* cc, int maxChannels, int txDbm);
void        drvWifiPowerSave(bool maxSaving);        // true = massimo risparmio, false = normale
void        drvWifiShutdown();                       // scollega e spegne la radio (prima del sonno profondo)

// canale in uso
int         drvWifiChannel();

// scansione delle reti vicine (blocca per qualche secondo)
int         drvWifiScanStart();                 // quante reti trovate (include le nascoste)
String      drvWifiScanSsid(int i);
int         drvWifiScanRssi(int i);
int         drvWifiScanChannel(int i);
bool        drvWifiScanOpen(int i);
void        drvWifiScanClear();
