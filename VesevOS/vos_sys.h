// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_sys.h
// CPU, RAM, disco, temperatura CPU, uptime, motivo reset, contatore avvii, contaore di vita.
#pragma once
#include <Arduino.h>

void     sysApplyCpuMode();         // applica il modo CPU (auto/fisso) della config
void     sysInit();                 // idle hook, task monitor, boot counter
void     sysSleep();                // sonno profondo: LED e Wi-Fi spenti, si riaccende solo con RESET
int      sysCpuPercent();           // 0..100
float    sysCpuTemp();              // gradi C (sensore interno del chip)
uint64_t sysUptimeSec();            // dall'avvio del sistema
uint32_t sysBootCount();
uint32_t sysLifeSec();              // contaore totale di vita della scheda (secondi, salvato ogni 10 min)
String   sysResetReason();
String   sysTempHistoryJson();      // ultimi 60 secondi
String   sysCpuHistoryJson();
String   sysStatusJson();           // tutto lo stato per la pagina
String   sysTasksText();            // elenco task per shell/web (testo)
String   sysTasksJson();            // elenco task per la pagina (JSON)
bool     sysTaskKill(const String& name, String& err);   // ferma un task della lista consentita
typedef void (*SysTaskStart)();
void     sysTaskRegister(const char* name, SysTaskStart start, SysTaskStart stop = nullptr);   // task che si puo fermare e riavviare (stop = arresto pulito)
bool     sysTaskRestart(const String& name, String& err);  // ferma (se gira) e fa ripartire
bool     sysTaskRunning(const char* name);
