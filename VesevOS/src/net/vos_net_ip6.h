// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_net_ip6.h
// IPv6 della Wi-Fi di casa: interruttore (di fabbrica spento), indirizzi dal router (automatico) e UN indirizzo fisso in piu (prefisso /64).
// Gateway e DNS IPv6: di fabbrica automatici (dal router); si possono scrivere a mano (IPv4 non si tocca). Il lavoro sull'hardware e in drivers/vos_drv_ip6*.
#pragma once
#include "../core/vos_errbuf.h"
#include "../core/vos_heapbuf.h"

#define NET6_JSON_MAX 1536

void net6Tick(bool clienteCollegato);                // dal task di rete, una volta al secondo (true = Wi-Fi di casa collegata): porta l'interfaccia a come dice la configurazione
bool net6Json(HeapBuf& out, bool collegato);         // stato per la pagina (collegato = Wi-Fi di casa collegata); false = manca la memoria o supera il tetto
bool net6Salva(bool acceso, bool fisso, const char* indirizzo, const char* gateway, const char* dns, ErrBuf& err);   // controlla, salva nella configurazione e applica (gateway e dns vuoti = automatici)
