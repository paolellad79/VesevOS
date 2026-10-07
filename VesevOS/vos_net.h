// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_net.h
// Wi-Fi: AP fisso 192.168.4.1 al primo avvio, client con DHCP o IP statico.
#pragma once
#include <Arduino.h>
#include "vos_common.h"

void     netInit();                 // avvia il task di rete
NetState netState();
String   netStatusJson();           // oggetto JSON con modo, ssid, ip, rssi...
String   netIpString();
String   netMac(bool ap);           // MAC Wi-Fi client (false) o Access Point (true), "AA:BB:..."
String   netInfoText();             // riassunto rete leggibile (comando net)
void     netReconfigure();          // applica cfg (dopo salvataggio)
void     netApplyServices();        // applica subito DHCP dell'hotspot, portale automatico e mDNS (senza riavvio)
void     netScanStart();            // avvia scansione asincrona
void     netTestStart(const String& ssid, const String& pass);   // prova il Wi-Fi di casa tenendo l'hotspot (guida)
String   netTestJson();             // {"state":0|1|2|3,"ip":..,"err":..}
String   netScanJson();             // {"running":bool,"list":[...]}
// Modalita aereo. exitMode: 0 al prossimo avvio, 1 dopo "param" secondi, 2 all'orario "param" (minuti dalla mezzanotte), 3 solo a mano
bool     netAirplaneOn(int exitMode, uint32_t param, String& err);
void     netAirplaneOff(const char* why);
bool     netAirplane();             // true se attiva
String   netAirplaneText();         // stato leggibile (shell)
bool     netCaptive();              // portale automatico attivo (modo AP)
void     netSetApChannel(int ch);   // canale dell'hotspot (rete schede: ricerca del gateway); solo canali ammessi
bool     netOnAp(uint32_t ip);      // l'indirizzo e nella rete dell'hotspot (192.168.4.x)?
