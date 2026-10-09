// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_drv_espnow.h
// STRATO 1 (driver): unico file che parla con ESP-NOW (esp_now_*) e legge canale e MAC della radio.
// Sa: accendere/spegnere, mandare un pacchetto a tutti (broadcast), dire il canale e il MAC, avvisare quando arriva un pacchetto.
// Non conosce il formato dei messaggi, le chiavi, i ruoli: sono del servizio (vos_mesh).
#pragma once
#include <Arduino.h>
#include "../core/vos_common.h"

#if VOS_WITH_MESH
typedef void (*EspNowRxFn)(const uint8_t* data, int len, int8_t rssi);   // chiamata dal task Wi-Fi: solo copiare e uscire

bool    drvEspNowRadioUp();                 // il Wi-Fi e acceso (serve per ESP-NOW)
bool    drvEspNowBegin(EspNowRxFn onRx);    // esp_now_init + ricezione + destinatario "tutti"
void    drvEspNowEnd();
bool    drvEspNowSend(const uint8_t* buf, size_t len);   // a tutti; sceglie da solo l'interfaccia (STA o AP) e rifa il destinatario se cambia
int     drvEspNowChannel();                 // canale della radio (0 = non noto)
void    drvEspNowMac(uint8_t out[6]);       // MAC dell'interfaccia in uso
bool    drvEspNowIsAp();                    // la radio e in modo hotspot
#endif
