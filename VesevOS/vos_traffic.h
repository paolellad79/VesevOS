// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_traffic.h
// Traffico Wi-Fi totale (byte in entrata e in uscita al secondo, ultimi 60 s). Solo contatori: nessun indirizzo, nessun contenuto.
#pragma once
#include <Arduino.h>

void   trafficTick();                      // da chiamare ogni secondo (aggancia i contatori alle interfacce Wi-Fi e campiona)
String trafficJson();                      // frammento JSON: "inBps":..,"outBps":..,"inHist":[..],"outHist":[..]
