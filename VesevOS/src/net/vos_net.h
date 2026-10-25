// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_net.h
// Wi-Fi: AP fisso 192.168.4.1 al primo avvio, client con DHCP o IP statico.
#pragma once
#include "../core/vos_errbuf.h"
#include <string>
#include "../core/vos_common.h"
#include "../core/vos_heapbuf.h"

void     netInit();                 // avvia il task di rete
NetState netState();
bool     netStatusJson(HeapBuf& out);   // oggetto JSON con modo, ssid, ip, rssi...; false = manca la memoria o supera il tetto
#define NET_STATUS_JSON_MAX 1024
void     netIpTo(char* out, size_t n);          // indirizzo IP in uso (AP o client), 16 byte bastano
void     netMacTo(bool ap, char* out, size_t n);   // MAC Wi-Fi client (false) o Access Point (true), "AA:BB:..." (18 byte)
std::string netInfoText();             // riassunto rete leggibile (comando net)
void     netReconfigure();          // applica cfg (dopo salvataggio)
void     netApplyServices();        // applica subito DHCP dell'hotspot, portale automatico e mDNS (senza riavvio)
void     netScanStart();            // avvia scansione asincrona
void     netTestStart(const char* ssid, const char* pass);   // prova il Wi-Fi di casa tenendo l'hotspot (guida)
bool     netTestJson(HeapBuf& out);   // {"state":0|1|2|3,"ip":..,"err":..}
#define NET_SCAN_JSON_MAX 8192
bool     netScanJson(HeapBuf& out);   // {"running":bool,"list":[...]}; false = manca la memoria o supera il tetto
bool     netScanRunning();            // la scansione e in corso?
// Modalita aereo. exitMode: 0 al prossimo avvio, 1 dopo "param" secondi, 2 all'orario "param" (minuti dalla mezzanotte), 3 solo a mano
bool     netAirplaneOn(int exitMode, uint32_t param, ErrBuf& err);
void     netAirplaneOff(const char* why);
bool     netAirplane();             // true se attiva
std::string netAirplaneText();         // stato leggibile (shell)
bool     netCaptive();              // portale automatico attivo (modo AP)
void     netSetApChannel(int ch);   // canale dell'hotspot (rete schede: ricerca del gateway); solo canali ammessi
bool     netOnAp(uint32_t ip);      // l'indirizzo e nella rete dell'hotspot (192.168.4.x)?
