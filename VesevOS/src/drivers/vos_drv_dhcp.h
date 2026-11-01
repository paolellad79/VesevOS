// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_drv_dhcp.h
// Informazioni extra del router via DHCP. Per ora solo l'opzione 15 (nome di dominio della rete, per esempio "lan").
// E un dato scritto dal router: non e fidato, non si salva in configurazione e si mostra soltanto (vedi da-fare-dhcp-informazioni.md).
#pragma once
#include <stddef.h>

// Copia in out il dominio ricevuto dal router, ripulito e validato. Ritorna la lunghezza; 0 = nessun dominio
// (router che non lo manda, scheda non collegata o indirizzo fisso: lo cancella il driver Wi-Fi quando l'indirizzo si perde).
size_t drvDhcpDomain(char* out, size_t n);

// Dimentica il dominio (nuova connessione o indirizzo fisso).
void   drvDhcpDomainClear();
