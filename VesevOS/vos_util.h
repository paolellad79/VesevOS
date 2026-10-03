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
bool   domainValid(const String& s);        // vuoto oppure nomi separati da punti (max 60)
