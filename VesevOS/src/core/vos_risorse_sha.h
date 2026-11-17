// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_risorse_sha.h
// SHA-256 a pezzi con mbedTLS (stesse chiamate di vos_util.cpp, acceleratore hardware dell'IDF) per le regole delle risorse (RisSha).
// Usato da vos_risorse.cpp (caricamento e controllo delle pagine .vrs) e da vos_risorse_file.cpp (file singoli con SHA256SUMS).
#pragma once
#include "mbedtls/sha256.h"
#include "vos_risorse_rule.h"

inline void risShaAvvia(void* c) {
  mbedtls_sha256_init((mbedtls_sha256_context*)c);
  mbedtls_sha256_starts((mbedtls_sha256_context*)c, 0);
}

inline void risShaAggiungi(void* c, const uint8_t* d, size_t n) {
  mbedtls_sha256_update((mbedtls_sha256_context*)c, d, n);
}

inline void risShaChiudi(void* c, uint8_t out[32]) {
  mbedtls_sha256_finish((mbedtls_sha256_context*)c, out);
  mbedtls_sha256_free((mbedtls_sha256_context*)c);
}

inline RisSha risShaMbed(mbedtls_sha256_context* ctx) { return RisSha{ctx, risShaAvvia, risShaAggiungi, risShaChiudi}; }
