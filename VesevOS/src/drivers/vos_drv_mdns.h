// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_drv_mdns.h
// STRATO 1 (driver): mDNS (nome.local) con il componente "mdns" di ESP-IDF. Lo stesso componente che usava ESPmDNS di Arduino.
#pragma once
#include <stdint.h>

bool drvMdnsBegin(const char* hostname);                           // avvia e imposta il nome (senza ".local")
bool drvMdnsAddService(const char* type, const char* proto, uint16_t port);   // per esempio ("http", "tcp", 80)
void drvMdnsEnd();                                                 // ferma e libera tutto
// Cerca l'indirizzo IPv4 di un nome mDNS ("stanza" o "stanza.local") e aspetta al massimo timeoutMs. ip: primo numero in alto.
// false = non trovato, scadenza, oppure mDNS non avviato (servizio spento).
bool drvMdnsResolve(const char* name, uint32_t timeoutMs, uint32_t& ip);
