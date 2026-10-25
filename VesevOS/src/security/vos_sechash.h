// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_sechash.h
// Piccoli attrezzi per la sicurezza senza String: SHA-256 e HMAC in esadecimale su buffer fissi, numeri casuali in esadecimale,
// confronto a tempo costante. Stesso risultato delle vecchie sha256Hex / randomHex / hexOf.
#pragma once
#include <initializer_list>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define SEC_HEX_BUF 65                      // SHA-256 in esadecimale: 64 caratteri + zero

// Un pezzo di testo da mettere nell'hash (un testo C, o un testo con la sua lunghezza)
struct SecPart {
  const char* p;
  size_t n;
  SecPart(const char* s) : p(s), n(s ? strlen(s) : 0) {}
  SecPart(const char* s, size_t len) : p(s), n(len) {}
};

void secHexOf(char* out, const uint8_t* b, size_t n);                    // byte -> esadecimale minuscolo; out: 2n+1 byte
void secSha256Hex(char out[SEC_HEX_BUF], std::initializer_list<SecPart> parts);   // SHA-256 dei pezzi messi in fila
bool secHmacSha256Hex(char out[SEC_HEX_BUF], const char* key, const char* msg);    // HMAC-SHA256 (false se mbedtls fallisce)
void secRandomHex(char* out, int bytes);                                 // out: 2*bytes+1 byte
bool secSameText(const char* a, const char* b);                          // confronto a tempo costante (lunghezze diverse = false)
