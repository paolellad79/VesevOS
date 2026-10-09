// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_fw.h
// Filtro IP (firewall semplice): decide CHI puo parlare con la scheda (pagina, API, server NTP).
// Modi: 0 spento, 1 solo la mia rete, 2 lista consentita, 3 lista bloccati.
// Sempre ammessi: la rete dell'hotspot (192.168.4.x) e la seriale USB (porta di emergenza).
// Fuori lista: la connessione si chiude senza risposta (la scheda resta "invisibile").
#pragma once
#include <Arduino.h>

bool   fwAllow(uint32_t ip, bool ntp);          // ip in ordine umano (primo numero nel byte alto)
bool   fwTryStart(uint32_t keepIp);             // modo prova: la regola nuova vale 2 minuti, poi torna la vecchia se non confermata
void   fwConfirm();                             // conferma la regola in prova
bool   fwTrying();                              // c'e una regola in prova?
uint32_t fwTryLeft();                           // secondi rimasti della prova
void   fwTick();                                // ogni secondo (task di rete): scadenza della prova
void   fwOff(const char* why);                  // uscita di emergenza (seriale, tasto BOOT 2-7 s)
String fwJson(uint32_t yourIp);                 // stato per la pagina
String fwText();
void   fwNoteRejected(uint32_t ip);             // contatore dei rifiutati
