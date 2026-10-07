// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_mesh.h
// Rete tra schede con ESP-NOW (servizio "mesh", SPENTO di fabbrica: la prima volta si accende a mano).
// - Ogni messaggio e firmato con la chiave comune (HMAC-SHA256): chi non ha la chiave non puo comandare.
// - Ripetizione automatica fino a 3 salti; i doppioni si scartano; numeri di sequenza contro la ripetizione.
// - Ruoli: 0 nodo, 1 gateway (annuncia il canale, pubblica tutti su MQTT), 2 sensore (manda solo lo stato).
// - Solo sui canali ammessi dal paese scelto. Il contenuto non e cifrato: niente dati personali o segreti.
#pragma once
#include <Arduino.h>

void   meshInit();                                  // registra il servizio; parte solo se gia attivato a mano (auto)
bool   meshStart(String& err);                      // accende (serve il Wi-Fi acceso e la chiave)
void   meshStop();
bool   meshRunning();
int    meshNodeCount();                             // schede vicine conosciute
bool   meshSendCmd(const String& to, const String& cmd, String& err);    // to = nome o MAC; esegue un'azione sull'altra scheda
bool   meshSendText(const String& to, const String& text, String& err);  // to = nome, MAC o "*" (tutti)
String meshNewKey();                                // 64 cifre esadecimali casuali
String meshJson();                                  // stato, nodi, ultimi messaggi
String meshText();
int    meshChannel();                               // canale in uso
