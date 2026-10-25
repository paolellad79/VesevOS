// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_crypt.h
// Cifratura del backup con i segreti: frase scelta da chi esporta -> chiave (PBKDF2-HMAC-SHA256) -> AES-256-GCM.
// Il file contiene solo testo esadecimale; senza la frase non si legge niente. Se la frase si perde, i segreti non si recuperano.
#pragma once
#include "../core/vos_errbuf.h"
#include "../core/vos_heapbuf.h"

#define CRYPT_MIN_PASS 10                                         // lunghezza minima della frase
#define CRYPT_OUT_MAX(plainLen) (2 * (size_t)(plainLen) + 160)     // tetto giusto per l'HeapBuf di cryptSeal
bool cryptIsSealed(const char* text);                             // il testo e un backup cifrato?
bool cryptSeal(const char* plain, const char* pass, HeapBuf& out, ErrBuf& err);     // out: HeapBuf con tetto CRYPT_OUT_MAX(strlen(plain))
bool cryptOpen(const char* sealed, const char* pass, HeapBuf& plain, ErrBuf& err);  // plain: HeapBuf con tetto almeno la meta del testo cifrato
