// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_drv_resolve.h
// STRATO 1 (driver): da nome a indirizzo IPv4 con il DNS (getaddrinfo di lwIP), CON SCADENZA. getaddrinfo da solo non ha un tempo
// massimo che si possa scegliere: qui gira in un piccolo task di aiuto e chi chiama aspetta al massimo timeoutMs.
// Per i nomi ".local" (mDNS) vedi drvMdnsResolve in vos_drv_mdns.h.
#pragma once
#include <stdint.h>

// true = trovato (ip: primo numero in alto, come ipParse). false = non trovato, errore o scadenza.
bool drvDnsResolve(const char* host, uint32_t timeoutMs, uint32_t& ip);
