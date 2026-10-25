// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_drv_boot.h
// STRATO 1 (driver): avvio della piattaforma e task principale. Prima lo faceva il core Arduino (initArduino e loopTask),
// ora lo facciamo noi sopra ESP-IDF. Lo usa solo src/vos_main.cpp.
#pragma once
#include <stdint.h>

typedef void (*DrvTaskFn)(void* arg);

// Prepara quello che serve prima di tutto il resto: conferma al bootloader che il programma parte (rollback), livello di log di fabbrica e memoria NVS
// (se e rovinata o di una versione nuova la cancella e la rifa, come faceva initArduino). false = la NVS non parte.
bool drvPlatformInit();

// Spegne i messaggi di log di un componente di ESP-IDF (per esempio "httpd").
void drvLogOff(const char* tag);

// Crea il task principale (quello che prima era il loopTask di Arduino) e si ricorda il suo handle per il watchdog.
// core: 0 o 1; priorita: 1 = quella piu bassa dopo l'idle. Ritorna false se il task non si crea (memoria).
bool drvMainTaskStart(DrvTaskFn fn, const char* name, uint32_t stackBytes, int priority, int core);

// Watchdog dei task (TWDT) per il task principale. Enable lo iscrive (si puo chiamare da qualunque task); Feed va chiamata a ogni giro
// del ciclo, da dentro il task principale. Se il task non gira per piu del tempo del watchdog (30 s di base, modificabile) il chip si riavvia.
bool drvLoopWdtEnable();
void drvLoopWdtFeed();

// Tempo del Task watchdog in secondi, a caldo (esp_task_wdt_reconfigure). Chiamare da UN solo punto alla volta (lo garantisce
// core/vos_wdset con un mutex). I task gia iscritti restano iscritti; il controllo dei task idle e il panico restano quelli
// scelti in compilazione. 0 = non cambiato (errore o valore non ammesso: l'intervallo lo controlla chi chiama).
bool drvTwdtSet(uint32_t sec);
uint32_t drvTwdtSec();                 // tempo attuale in secondi
uint32_t drvIntWdtMs();                // watchdog di interruzione: valore di compilazione, solo lettura
