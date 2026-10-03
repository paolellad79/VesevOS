// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// VesevOS - vos_net.h
// Wi-Fi: AP fisso 192.168.4.1 al primo avvio, client con DHCP o IP statico.
#pragma once
#include <Arduino.h>
#include "vos_common.h"

void     netInit();                 // avvia il task di rete
NetState netState();
String   netStatusJson();           // oggetto JSON con modo, ssid, ip, rssi...
String   netIpString();
void     netReconfigure();          // applica cfg (dopo salvataggio)
void     netScanStart();            // avvia scansione asincrona
String   netScanJson();             // {"running":bool,"list":[...]}
