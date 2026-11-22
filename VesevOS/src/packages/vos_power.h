// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_power.h
// Risparmio energia: (1) Wi-Fi a risparmio massimo (la scheda resta raggiungibile, piu lenta);
// (2) sonno profondo a cicli (sveglia X minuti, dorme Y minuti; si sveglia con il timer o con RESET).
#pragma once
#include "../core/vos_errbuf.h"
#include "../core/vos_heapbuf.h"
#include <stdint.h>
#include <string>

constexpr size_t POWER_JSON_MAX = 256;
void   powerBegin();                       // all'avvio: conta i risvegli dal sonno
void   powerTick();                        // dal ciclo principale
void   powerSleepNow(uint32_t seconds);    // sonno profondo subito: 0 = fino a RESET
uint32_t powerSecondiAllOra(unsigned minuti);   // secondi fino alla prossima volta che l'ora locale segna HH:MM (minuti dalla mezzanotte); 0 = ora non valida
bool   powerRipresaPresente();   // c'e un tasto di ripresa installato? (il piedino non si vede)
bool   powerStandbyControlla(bool profondo, uint32_t sec, bool conRipresa, ErrBuf& err);   // controlli senza dormire
#define POWER_LEGGERO_MAX_S 0    // secondi massimi del sonno leggero: 0 = SOSPESO (22/11/2026, prove sull'S3: la memoria risulta rovinata dopo ogni sonno leggero); lo standby leggero diventa profondo. Per riattivarlo basta un valore maggiore (es. 30)
bool   powerStandby(bool profondo, uint32_t sec, ErrBuf& err);   // controlla e PRENOTA lo standby (parte un secondo dopo dal ciclo principale: la risposta esce prima); false = motivo in err
void   powerSet(int mode, uint32_t awakeMin, uint32_t sleepMin, ErrBuf& err);   // 0 spento, 1 Wi-Fi, 2 cicli
bool powerJson(HeapBuf& out);   // stato in JSON (circa 120 byte); false = manca la memoria o supera il tetto
std::string powerText();
