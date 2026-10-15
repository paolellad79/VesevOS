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
uint32_t drvRecoverySize();         // dimensione della partizione recovery in byte (0 se manca)
bool   drvRecoveryRead(uint32_t off, uint8_t* buf, size_t n);   // legge dalla partizione recovery
// Riscrive il recovery con un'immagine gia controllata. Prima si assicura che all'avvio parta VesevOS (otadata su app0):
// cosi se la corrente salta a meta VesevOS parte lo stesso. Il primo blocco (con l'intestazione) si scrive per ultimo,
// poi si rilegge tutto e si confronta. Risposta: 0 = fatto, altrimenti uno dei codici REC_W_*.
enum { REC_W_OK = 0, REC_W_NOPART, REC_W_SIZE, REC_W_RUNNING, REC_W_BOOT, REC_W_ERASE, REC_W_WRITE, REC_W_VERIFY };
int    drvRecoveryWrite(const uint8_t* d, uint32_t n);
