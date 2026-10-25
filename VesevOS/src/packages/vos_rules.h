// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_rules.h
// Automazioni: regole "Quando ... Se ... Allora ..." (sveglie, timer, eventi), anche senza rete.
//
// File /rules.txt, una regola per riga:   attiva|nome|quando|se|azioni
//   attiva : 1 oppure 0
//   quando : time HH:MM LMMGVSD (7 cifre 0/1, lunedi..domenica) | every N | after N | boot | wifi up | wifi down | temp N
//   se     : -  |  between HH:MM HH:MM  |  day LMMGVSD
//   azioni : separate da ";" -> led-color RRGGBB | led state|heartbeat|fixed|off | led-bright N
//            | gpio N 0|1 | wait N | note testo | reboot | ntp sync | airplane on|off ... | mqtt <argomento> <testo>
#pragma once
#include "../core/vos_errbuf.h"
#include "../core/vos_heapbuf.h"
#include "../drivers/vos_drv_string.h"
#include <string>
#include <string_view>

void   rulesInit();                                  // carica /rules.txt e avvia il task "rules"
#define RULES_READ_MAX 32768                          // tetto del testo letto dal file (l'editor dei file arriva a 32 KB)
bool   rulesText(HeapBuf& out);                       // contenuto del file in out (vuoto se il file non c'e); false = manca la memoria o supera il tetto
bool   rulesSave(std::string_view text, ErrBuf& err);   // controlla, salva e ricarica
std::string rulesStatusJson();                       // stato di ogni regola (ultima esecuzione, conteggio, antiloop)
bool   rulesRunNow(int index, ErrBuf& err);          // "prova ora"
int    rulesCount();
bool   rulesAction(const char* action, ErrBuf& err, const char* who = "MQTT");   // esegue subito UNA azione (controllata; "wait" non ammessa) - MQTT, rete schede
