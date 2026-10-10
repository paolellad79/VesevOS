// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_drv_ota.h
// STRATO 1 (driver): unico file che usa esp_partition_* / esp_ota_* (partizioni di avvio).
// Strada B2 dell'aggiornamento: la scheda ha un programma di RECUPERO (partizione "factory") e VesevOS in app0.
#pragma once
#include <Arduino.h>

bool   drvRecoveryPresent();        // esiste una partizione recovery con un'immagine valida?
uint32_t drvRecoverySizeKB();       // dimensione della partizione recovery (0 se manca)
bool   drvRecoveryEnter();          // fa partire il recovery al prossimo avvio (azzera otadata); poi serve il riavvio
