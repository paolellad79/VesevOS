// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_rec.h
// Registratore di dati: legge le fonti scelte (fino a 4 periferiche per nome, temperatura del chip, RAM libera), ognuna con il suo intervallo
// (o quello predefinito "sec"), tiene gli ultimi 120 punti in RAM per il grafico e scrive una riga CSV su flash (/var/lib/rec) o microSD (/media/sd/rec).
// Una riga si scrive quando almeno una fonte e scaduta; le colonne delle altre restano vuote.
// In piu traccia le AZIONI sul dispositivo (uscite, PWM, LED, riavvii...) in eventi.csv (vos_rec_ev.cpp).
// Le righe si accumulano in RAM e si scrivono a blocchi, per non consumare la flash. Spento di fabbrica.
// Motore: task del firmware, uguale su tutte le schede. Le capacita del chip (ULP, CPU LP, PCNT, memoria RTC) sono solo dichiarate.
#pragma once
#include "vos_rec_cfg.h"
#include "../core/vos_errbuf.h"
#include "../core/vos_heapbuf.h"
#include <stdint.h>
#include <string>

void recBegin();                                            // avvia il task (da chiamare dopo la lettura della configurazione)
void recSettingsGet(RecSettings& s);                        // impostazioni attuali (dalla configurazione)
void recSet(const RecSettings& s, ErrBuf& err);             // controlla, applica alla configurazione (chi chiama poi salva) e svuota il grafico
void recClear(ErrBuf& err);                                 // cancella i file del registratore e i punti in RAM
bool recJson(HeapBuf& out, bool conPunti);                  // stato in JSON; false = manca la memoria o supera il tetto (mai troncato)
std::string recText();                                      // per la shell
std::string recAzioniText();                                // per la shell: categorie, conteggi e ultime azioni
void recFlushNow(uint32_t maxMs);                           // scrive subito campioni e azioni in attesa (prima di un riavvio o del sonno); aspetta il blocco al massimo maxMs
