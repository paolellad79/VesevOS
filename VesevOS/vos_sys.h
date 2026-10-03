// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// VesevOS - vos_sys.h
// CPU, RAM, disco, temperatura CPU, uptime, motivo reset, contatore avvii.
#pragma once
#include <Arduino.h>

void     sysApplyCpuMode();         // applica il modo CPU (auto/fisso) della config
void     sysInit();                 // idle hook, task monitor, boot counter
int      sysCpuPercent();           // 0..100
float    sysCpuTemp();              // gradi C (sensore interno del chip)
uint64_t sysUptimeSec();            // dall'avvio del sistema
uint32_t sysBootCount();
String   sysResetReason();
String   sysTempHistoryJson();      // ultimi 60 secondi
String   sysCpuHistoryJson();
String   sysStatusJson();           // tutto lo stato per la pagina
String   sysTasksText();            // elenco task per shell/web
