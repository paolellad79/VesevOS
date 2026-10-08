// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_util.h
// Funzioni di utilita: IP, uptime, testo pulito (solo ASCII), hash.
#pragma once
#include <Arduino.h>

bool   ipParse(const String& s, uint32_t& out);   // "192.168.1.5" -> numero
String ipToStr(uint32_t ip);
bool   maskValid(uint32_t mask);                  // prefisso da 8 a 30
String uptimeStr(uint64_t seconds);               // anni/mesi/giorni/ore/min/sec
String cleanAscii(const String& s);               // toglie caratteri strani
String jsonEscape(const String& s);
String sha256Hex(const String& s);
String randomHex(int bytes);
bool   hostnameValid(const String& s);      // 1-32: lettere, numeri, trattino
uint32_t utilStackSpare(uint32_t total, uint32_t freeMin);        // byte di stack che si potrebbero togliere tenendo il 25% libero (a passi di 256); 0 se total e 0
bool   utilEditKey(String& buf, uint8_t& esc, char ch, String& echo, size_t maxLen);   // un tasto sulla riga in digitazione: Backspace cancella, Ctrl+U/Ctrl+C svuotano, i tasti freccia (ESC...) si scartano; echo = cosa mostrare al terminale
bool   utilHostOk(const String& s);                                // nome di server valido: 1-60 caratteri, solo lettere, numeri, punto e trattino
bool   utilTimeSyncOk(uint32_t got, uint32_t expected, uint8_t refused);   // l'ora ricevuta e credibile? (expected = ora attesa dall'ultima buona, 0 = nessuna)
bool   utilTakeArg(const String& s, int& pos, String& out);   // legge una parola da pos; tra virgolette " " puo avere spazi (\" per una virgoletta); false se non c'e altro
bool   domainValid(const String& s);        // vuoto oppure nomi separati da punti (max 60)
