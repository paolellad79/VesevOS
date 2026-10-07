// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_audit.h
// Controllo della configurazione ("semaforo" della scheda) e registro delle modifiche.
//  ROSSO  = valore vietato (legge o sicurezza): viene corretto subito, l'allarme resta finche la causa c'e.
//  GIALLO = valore permesso ma rischioso: resta, con un avviso fisso.
//  INFO   = solo una riga nel registro (chi ha cambiato cosa).
#pragma once
#include <Arduino.h>

enum AuditLevel : uint8_t { AUD_INFO = 0, AUD_YELLOW = 1, AUD_RED = 2 };

void   auditFix();                                   // corregge i valori vietati (chiamata da cfgSave)
void   auditRefresh();                               // ricalcola gli allarmi
void   auditDiff(const String& oldTxt, const String& newTxt, const String& origin);   // registro delle modifiche
void   auditEvent(AuditLevel lv, const String& key, const String& text);              // evento da altri moduli
void   auditClear(const String& key);                // l'evento non c'e piu
void   auditTick();                                  // ogni minuto: file cambiato fuori dal pannello?
int    auditCount(AuditLevel lv);                    // allarmi attivi di quel livello (non visti)
String auditJson();                                  // {"alarms":[...],"history":[...]}
String auditText();
bool   auditAck(int n);                              // presa visione; 0 = tutti (se la causa resta, l'allarme resta in elenco)
void   auditInit();
