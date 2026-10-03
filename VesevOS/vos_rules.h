// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_rules.h
// Automazioni: regole "Quando ... Se ... Allora ..." (sveglie, timer, eventi), anche senza rete.
//
// File /rules.txt, una regola per riga:   attiva|nome|quando|se|azioni
//   attiva : 1 oppure 0
//   quando : time HH:MM LMMGVSD (7 cifre 0/1, lunedi..domenica) | every N | after N | boot | wifi up | wifi down | temp N
//   se     : -  |  between HH:MM HH:MM  |  day LMMGVSD
//   azioni : separate da ";" -> led-color RRGGBB | led state|heartbeat|fixed|off | led-bright N | led2 on|off|heartbeat
//            | gpio N 0|1 | wait N | note testo | reboot | ntp sync
#pragma once
#include <Arduino.h>

void   rulesInit();                                  // carica /rules.txt e avvia il task "rules"
String rulesText();                                  // contenuto del file
bool   rulesSave(const String& text, String& err);   // controlla, salva e ricarica
String rulesStatusJson();                            // stato di ogni regola (ultima esecuzione, conteggio, antiloop)
bool   rulesRunNow(int index, String& err);          // "prova ora"
int    rulesCount();
