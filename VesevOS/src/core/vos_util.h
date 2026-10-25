// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_util.h
// Funzioni di utilita: IP, uptime, testo pulito (solo ASCII), hash.
#pragma once
#include <stddef.h>
#include <stdint.h>
#include <string_view>
#include "vos_errbuf.h"

bool   ipParse(std::string_view s, uint32_t& out);   // "192.168.1.5" -> numero (spazi ai lati ammessi)
void   ipTo(uint32_t ip, char* out, size_t n);   // lo stesso in un buffer (16 byte bastano)
struct IpText { char s[16]; explicit IpText(uint32_t ip) { ipTo(ip, s, sizeof s); } };   // per i printf: IpText(ip).s
bool   maskValid(uint32_t mask);                  // prefisso da 8 a 30
void   uptimeTo(uint64_t seconds, char* out, size_t n);   // come uptimeStr, in un buffer (max 64 byte bastano)
void   durStr(uint64_t seconds, char* out, size_t n);   // durata: "45 s", "12 min", "3 h 5 min", "5 g 23 h 11 min" (giorni al posto di mesi/anni)
size_t cleanAscii(std::string_view s, char* out, size_t cap);   // toglie i \r, i caratteri strani diventano '?'; scrive in out (zero finale, tagliato a cap-1); ritorna i caratteri scritti
void   sha256HexOf(std::string_view data, char out[65]);        // SHA-256 in esadecimale minuscolo (64 cifre + zero)
bool   utilNameOk(const char* s, size_t n);      // nome corto valido (script, schede .vdp): 1-24 caratteri a-z 0-9 - _
bool   utilLineErr(ErrBuf& err, int line, const char* msg);   // err = "Riga N: msg"; sempre false (per i lettori di file a righe)
bool   hostnameValid(std::string_view s);      // 1-32: lettere, numeri, trattino
uint32_t utilStackSpare(uint32_t total, uint32_t freeMin);        // byte di stack che si potrebbero togliere tenendo il 25% libero (a passi di 256); 0 se total e 0
bool   utilHostOk(std::string_view s);                               // nome di server valido: 1-60 caratteri, solo lettere, numeri, punto e trattino
bool   utilTimeSyncOk(uint32_t got, uint32_t expected, uint8_t refused);   // l'ora ricevuta e credibile? (expected = ora attesa dall'ultima buona, 0 = nessuna)
bool   utilTakeArg(std::string_view s, int& pos, char* out, size_t cap);   // legge una parola da pos in out (tagliata a cap-1); tra virgolette " " puo avere spazi (\" per una virgoletta); false se non c'e altro
bool   domainValid(std::string_view s);       // vuoto oppure nomi separati da punti (max 60)
bool   utilSvgSafe(std::string_view svg, size_t maxLen);   // true se e un solo <svg> con forme semplici (niente script, collegamenti, stili, immagini), lungo al massimo maxLen
