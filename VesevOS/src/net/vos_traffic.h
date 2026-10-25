// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_traffic.h
// Traffico Wi-Fi totale (byte in entrata e in uscita al secondo, ultimi 60 s). Solo contatori: nessun indirizzo, nessun contenuto.
#pragma once
#include <stddef.h>
#include "../core/vos_strbuf.h"

void   trafficTick();                      // da chiamare ogni secondo (aggancia i contatori alle interfacce Wi-Fi e campiona)
#define TRAFFIC_JSON_MAX 1400              // capacita che basta sempre: 2 elenchi da 60 numeri a 10 cifre + il resto
// frammento JSON: "trafHook":..,"inHist":[..],"outHist":[..]  Scrive in out (capacita cap) e ritorna i caratteri scritti; se non ci sta, tronca (il chiamante controlla)
size_t trafficJson(char* out, size_t cap);
