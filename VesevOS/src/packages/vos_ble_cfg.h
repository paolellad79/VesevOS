// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_ble_cfg.h
// STRATO 4 (servizio): i comandi di prima configurazione ricevuti dal telefono via Bluetooth.
// Non conosce la radio: riceve un testo e restituisce la risposta. Usa le funzioni del nucleo (configurazione, rete).
// Comandi: wifi <rete>|<password>, country <XX>, name <nome>, mesh <ruolo> <chiave>, status, done.
#pragma once
#include <Arduino.h>
#include "../core/vos_common.h"

#if VOS_WITH_BLE
// esegue un comando; ritorna il testo di risposta. stopAfter = true se dopo la risposta il Bluetooth va spento ("done").
String bleCfgRun(String cmd, bool& stopAfter);
#endif
