// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_power.h
// Risparmio energia: (1) Wi-Fi a risparmio massimo (la scheda resta raggiungibile, piu lenta);
// (2) sonno profondo a cicli (sveglia X minuti, dorme Y minuti; si sveglia con il timer o con RESET).
#pragma once
#include <Arduino.h>

void   powerBegin();                       // all'avvio: conta i risvegli dal sonno
void   powerTick();                        // dal ciclo principale
void   powerSleepNow(uint32_t seconds);    // sonno profondo subito: 0 = fino a RESET
void   powerSet(int mode, uint32_t awakeMin, uint32_t sleepMin, String& err);   // 0 spento, 1 Wi-Fi, 2 cicli
String powerJson();
String powerText();
