// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_sha256_sw.h
// SHA-256 solo software (FIPS 180-4), scritta qui. Serve come riferimento per misurare e verificare l'acceleratore hardware:
// il firmware usa mbedTLS (hardware) per le password, questa NON la sostituisce. Nessuna dipendenza da Arduino.
#pragma once
#include <stddef.h>
#include <stdint.h>

// Calcola l'SHA-256 di len byte; out e di 32 byte.
void sha256SwHash(const uint8_t* data, size_t len, uint8_t out[32]);
