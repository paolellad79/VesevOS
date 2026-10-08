// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_crypt.h
// Cifratura del backup con i segreti: frase scelta da chi esporta -> chiave (PBKDF2-HMAC-SHA256) -> AES-256-GCM.
// Il file contiene solo testo esadecimale; senza la frase non si legge niente. Se la frase si perde, i segreti non si recuperano.
#pragma once
#include <Arduino.h>

#define CRYPT_MIN_PASS 10                                         // lunghezza minima della frase
bool cryptIsSealed(const String& text);                           // il testo e un backup cifrato?
bool cryptSeal(const String& plain, const String& pass, String& out, String& err);
bool cryptOpen(const String& sealed, const String& pass, String& plain, String& err);
