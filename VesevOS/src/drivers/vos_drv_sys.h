// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_drv_sys.h
// STRATO 1 (driver): riavvio, numeri casuali, informazioni sul chip e sul programma, temperatura interna.
// drvSysRestart() lo chiama solo sysReboot()/wdReboot() (core/vos_wd), che prima salvano il motivo.
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <esp_random.h>

void drvSysRestart();
// Numero casuale di 32 bit dal generatore del chip (hardware).
inline uint32_t drvRandom32() { return esp_random(); }
// Riempie un buffer di byte casuali dal generatore del chip.
inline void drvRandomFill(void* buf, size_t len) { esp_fill_random(buf, len); }

const char* drvChipModel();        // per esempio "ESP32-S3"
uint32_t    drvChipRevision();     // 100 * maggiore + minore (come esp_chip_info)
uint32_t    drvChipCores();
const char* drvSdkVersion();       // versione di ESP-IDF
uint32_t    drvFlashSize();        // dimensione fisica della flash in byte (0 se non si legge)
void        drvBuildDateTo(char* out, size_t n);   // data e ora in cui il programma e stato compilato ("Oct 23 2026 10:15:00"; "" se non si legge)
float       drvTempC();            // temperatura interna del chip in gradi C (NAN se il sensore non risponde)
