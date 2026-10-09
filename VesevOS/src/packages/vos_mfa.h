// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_mfa.h
// Seconda verifica all'accesso (TOTP, RFC 6238) con codici di recupero, e prova di lavoro anti-bot.
#pragma once
#include <Arduino.h>
#include "../core/vos_common.h"

// ---- prova di lavoro ----
bool   powCheck(const String& nonce, const String& answer);   // sha256(nonce:answer) con almeno cfg.powBits zeri iniziali

#if VOS_WITH_MFA
// ---- attivazione (utente gia entrato) ----
String mfaBegin(int idx, const String& host);                 // crea un segreto provvisorio: {"uri":..,"key":..}
bool   mfaConfirm(int idx, const String& code, String& recovery, String& err);   // codice giusto -> MFA acceso + codici di recupero
void   mfaOff(int idx);                                       // spegne e cancella segreto e codici
bool   mfaNewRecovery(int idx, String& recovery);             // 8 codici nuovi (vecchi annullati)
bool   mfaOn(int idx);
String mfaJson(int only = -1);                                // stato per ogni utente (only = solo quell'utente)
int    mfaOffAdmins();                                        // tasto BOOT 8 s: spegne l'MFA degli amministratori (ritorna quanti)
// ---- accesso in due passi ----
String mfaLoginToken(int idx, bool& blocked);                 // dopo la password giusta: gettone di 120 s ("" se bloccato)
// ritorna 0 ok (idx), 1 codice errato, 2 gettone scaduto, 3 ora non valida (serve ora dal browser), 4 bloccato
int    mfaLoginCheck(const String& tok, const String& code, uint32_t browserNow, int& idx, String& err);
bool   mfaLoginNoTime(const String& tok);                     // il gettone e per una scheda senza ora valida?
// ---- prova interna (stub) ----
String mfaTotpAt(const String& hexSecret, uint32_t counter);  // 6 cifre
#else
// Package spento (VOS_WITH_MFA = 0): funzioni vuote, il resto del codice non cambia.
inline String mfaBegin(int, const String&) { return "{}"; }
inline bool   mfaConfirm(int, const String&, String&, String& err) { err = "MFA non presente in questo firmware"; return false; }
inline void   mfaOff(int) {}
inline bool   mfaNewRecovery(int, String&) { return false; }
inline bool   mfaOn(int) { return false; }
inline String mfaJson(int = -1) { return "[]"; }
inline int    mfaOffAdmins() { return 0; }
inline String mfaLoginToken(int, bool& blocked) { blocked = false; return ""; }
inline int    mfaLoginCheck(const String&, const String&, uint32_t, int&, String& err) { err = ""; return 0; }
inline bool   mfaLoginNoTime(const String&) { return false; }
inline String mfaTotpAt(const String&, uint32_t) { return ""; }
#endif
