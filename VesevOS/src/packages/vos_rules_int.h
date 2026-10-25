// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_rules_int.h: tipi e stato delle Automazioni comuni a vos_rules.cpp e vos_rules_state.cpp (non usare altrove).
#pragma once
#include "vos_rules.h"
#include <string>
#include <time.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#define RULES_FILE   "/rules.txt"
#define MAX_RULES    24
#define MAX_RUNS     4
#define MAX_TEXT     4096
#define MAX_ACTS     8
#define LOOP_MAX     10      // piu di 10 esecuzioni in un minuto = antiloop

enum { TK_TIME = 0, TK_EVERY, TK_AFTER, TK_BOOT, TK_WIFIUP, TK_WIFIDOWN, TK_TEMP };
enum { CK_NONE = 0, CK_BETWEEN, CK_DAY };

struct Rule {
  bool     on;
  char     name[25];
  uint8_t  tk;
  uint16_t a;            // minuti del giorno (time), secondi (every/after), gradi (temp)
  uint8_t  dmask;        // giorni (bit 0 = lunedi)
  uint8_t  ck;
  uint16_t c1, c2;       // fascia oraria (minuti)
  uint8_t  cmask;
  std::string acts;      // azioni separate da ";"
  // stato in esecuzione
  uint32_t lastMs;
  int32_t  lastMin;
  bool     done, armed, off;
  uint32_t nfire;
  time_t   lastEpoch;
  uint32_t ring[LOOP_MAX];
  uint8_t  ringN;
};

struct Run { bool act; int8_t rule; uint8_t step; uint32_t wake; };

namespace vosrules {
extern Rule* g_r;                 // tabella delle regole (nullptr con 0 regole)
extern int   g_n;
extern Run   g_run[MAX_RUNS];
extern SemaphoreHandle_t g_mx;
void ensureMx();
}
using namespace vosrules;
