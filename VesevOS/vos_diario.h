// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_diario.h
// Diario dei riavvii: ultimi 20 avvii con motivo, durata precedente, RAM minima, task piu attivo e ultima riga grave.
// Due parti: un'istantanea in memoria RTC (sopravvive a watchdog, crash e reset, non allo spegnimento) e un file in LittleFS.
// Solo dati tecnici: niente IP, MAC, nomi Wi-Fi o utenti. Resta sulla scheda.
#pragma once
#include <Arduino.h>

void   diaryEarly();                          // PRIMA di ogni altra cosa in setup(): copia l'istantanea lasciata dall'avvio precedente
void   diaryInit();                           // dopo LittleFS e sysInit: registra QUESTO avvio nel diario e scrive la riga di log
void   diaryTick(uint32_t upSec, uint32_t minHeap, const char* topTask, int topPct, uint32_t mhz);   // ogni secondo dal task monitor (scrive in RTC ogni 10 s)
void   diaryStage(const char* stage);         // fase di avvio in corso (es. "net"): se la scheda si blocca, il diario dice dove
void   diaryNote(const char* line);           // ultima riga grave (errore/attenzione) per l'istantanea
String diaryText(int n, bool withLast);       // elenco leggibile, piu recente per primo ("" se vuoto)
int    diaryCount();
void   diaryClear();
