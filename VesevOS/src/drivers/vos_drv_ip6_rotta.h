// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_drv_ip6_rotta.h
// STRATO 1 (driver): gateway e DNS IPv6 a mano, per l'interfaccia client (Wi-Fi di casa). Di fabbrica sono automatici (li da il router).
// - Gateway: lwIP chiede il prossimo salto a due ganci (LWIP_HOOK_IP6_ROUTE e LWIP_HOOK_ND6_GET_GW) che in ESP-IDF 5.5 sono "default (weak)":
//   noi li ridefiniamo in vos_drv_ip6_rotta.cpp, senza cambiare lo sdkconfig. Senza gateway fisso rispondono NULL e lwIP fa come prima.
// - DNS: va nello slot "di riserva" (ESP_NETIF_DNS_FALLBACK, ultimo) e si usa solo se i DNS IPv4 non rispondono. Gli slot principale e
//   secondario (DNS IPv4 del router o scritti da noi) non si toccano.
// Senza CONFIG_LWIP_IPV6 (recovery) le funzioni esistono ma non fanno nulla.
#pragma once
#include <stddef.h>
#include "vos_drv_ip6.h"

// Controlli: il gateway deve essere raggiungibile direttamente: indirizzo locale (fe80::/10) oppure nella stessa rete /64 di un indirizzo
// della scheda ("fisso" = l'indirizzo fisso che si sta salvando, "" se non c'e). Il DNS: solo globale o privato. norm = forma normale.
DrvIp6Esito drvIp6ControllaGw(const char* in, const char* fisso, char* norm, size_t n);
DrvIp6Esito drvIp6ControllaDns(const char* in, char* norm, size_t n);

// Applicazione. Testo vuoto = automatico (come il router). Ritorna false se lwIP non ha accettato la richiesta (si riprova).
bool drvIp6ImpostaGateway(const char* norm);
bool drvIp6ImpostaDns(const char* norm);
