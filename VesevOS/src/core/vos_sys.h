// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_sys.h
// CPU, RAM, disco, temperatura CPU, uptime, motivo reset, contatore avvii, contaore di vita.
#pragma once
#include "vos_errbuf.h"
#include "../drivers/vos_drv_string.h"
#include "vos_heapbuf.h"

#define SYS_STATUS_JSON_MAX 6144     // tetto del JSON di stato (ne servono ~2,5 KB)
#define SYS_TASKS_JSON_MAX  6144
#define SYS_TASKS_TEXT_MAX  3072

void     sysApplyCpuMode();         // applica il modo CPU (auto/fisso) della config
void     sysInit();                 // idle hook, task monitor, boot counter
void     sysSleep();                // sonno profondo: LED e Wi-Fi spenti, si riaccende solo con RESET
int      sysCpuPercent();           // 0..100
int      sysCpuCorePercent(int core);   // carico del core 0 o 1 (0..100; altro numero = 0)
float    sysCpuTemp();              // gradi C (sensore interno del chip)
uint64_t sysUptimeSec();            // dall'avvio del sistema
uint32_t sysBootCount();
uint32_t sysLifeSec();              // contaore totale di vita della scheda (secondi, salvato ogni 10 min)
void     sysLifeSave();             // salva subito il contaore (lo usa l'antiloop del watchdog prima di riavviare)
const char* sysResetText();             // motivo dell'ultimo reset (testo tradotto, vale per tutta la vita del programma)
void     sysResetNameTo(int reason, char* out, size_t n);   // nome (tradotto) di un motivo di reset di esp_reset_reason()
void     sysTopTask(char* name, size_t n, int* pct);   // task (non IDLE) piu attivo nell'ultimo secondo
// JSON e testi scritti nel HeapBuf del chiamante: false se manca la memoria o si supera il tetto (allora non mandare il testo).
bool     sysTempHistoryJson(HeapBuf& out);      // ultimi 60 secondi
bool     sysCpuHistoryJson(HeapBuf& out);
bool     sysStatusJson(HeapBuf& out);           // tutto lo stato per la pagina
bool     sysTasksText(HeapBuf& out);            // elenco task per shell/web (testo)
bool     sysTasksJson(HeapBuf& out);            // elenco task per la pagina (JSON)
bool     sysTaskKill(const char* name, char* err, size_t errLen);      // ferma un task della lista consentita
typedef void (*SysTaskStart)();
void     sysTaskRegister(const char* name, SysTaskStart start, SysTaskStart stop = nullptr);   // task che si puo fermare e riavviare (stop = arresto pulito)
bool     sysTaskRestart(const char* name, char* err, size_t errLen);   // ferma (se gira) e fa ripartire
bool     sysTaskRunning(const char* name);
