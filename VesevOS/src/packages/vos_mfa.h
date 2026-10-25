// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_mfa.h
// Seconda verifica all'accesso (TOTP, RFC 6238) con codici di recupero, e prova di lavoro anti-bot.
#pragma once
#include "../core/vos_errbuf.h"
#include "../core/vos_heapbuf.h"
#include "../core/vos_common.h"

#define MFA_REC_BUF   80                                      // 8 codici "XXXX-XXXX" separati da spazio + zero
#define MFA_TOK_BUF   33                                      // gettone di accesso: 32 caratteri + zero
#define MFA_CODE_BUF  7                                       // codice TOTP: 6 cifre + zero
#define MFA_JSON_MAX  2048                                    // tetto dei testi JSON di questo modulo

// ---- prova di lavoro ----
bool   powCheck(const char* nonce, const char* answer);       // sha256(nonce:answer) con almeno cfg.powBits zeri iniziali

#if VOS_WITH_MFA
// ---- attivazione (utente gia entrato) ----
bool   mfaBegin(int idx, const char* host, HeapBuf& j);       // crea un segreto provvisorio e scrive {"uri":..,"key":..}; false se l'utente non c'e
bool   mfaConfirm(int idx, const char* code, char recovery[MFA_REC_BUF], ErrBuf& err);   // codice giusto -> MFA acceso + codici di recupero
void   mfaOff(int idx);                                       // spegne e cancella segreto e codici
bool   mfaNewRecovery(int idx, char recovery[MFA_REC_BUF]);   // 8 codici nuovi (vecchi annullati)
bool   mfaOn(int idx);
void   mfaJson(int only, HeapBuf& j);                         // stato per ogni utente (only = solo quell'utente, -1 = tutti)
int    mfaOffAdmins();                                        // tasto BOOT 8 s: spegne l'MFA degli amministratori (ritorna quanti)
// ---- accesso in due passi ----
bool   mfaLoginToken(int idx, char tok[MFA_TOK_BUF]);         // dopo la password giusta: gettone di 120 s (false = bloccato, ora non valida)
// ritorna 0 ok (idx), 1 codice errato, 2 gettone scaduto, 3 ora non valida (serve ora dal browser), 4 bloccato
int    mfaLoginCheck(const char* tok, const char* code, uint32_t browserNow, int& idx, ErrBuf& err);
bool   mfaLoginNoTime(const char* tok);                       // il gettone e per una scheda senza ora valida?
// Come mfaLoginToken, per la seriale: senza ora valida il gettone accetta solo i codici di recupero (mai bloccato del tutto, mai ora dal browser).
// Il codice si controlla poi con mfaLoginCheck(tok, code, 0, ...): stesse regole del web (6 cifre TOTP o codice di recupero usa e getta, 5 tentativi).
bool   mfaLoginTokenSerial(int idx, char tok[MFA_TOK_BUF]);
// ---- prova interna (stub) ----
void   mfaTotpAt(const char* hexSecret, uint32_t counter, char out[MFA_CODE_BUF]);   // 6 cifre (vuoto se il segreto non e valido)
#else
// Package spento (VOS_WITH_MFA = 0): funzioni vuote, il resto del codice non cambia.
inline bool   mfaBegin(int, const char*, HeapBuf& j) { j.add("{}"); return true; }
inline bool   mfaConfirm(int, const char*, char*, ErrBuf& err) { err = "MFA non presente in questo firmware"; return false; }
inline void   mfaOff(int) {}
inline bool   mfaNewRecovery(int, char*) { return false; }
inline bool   mfaOn(int) { return false; }
inline void   mfaJson(int, HeapBuf& j) { j.add("[]"); }
inline int    mfaOffAdmins() { return 0; }
inline bool   mfaLoginToken(int, char* tok) { tok[0] = 0; return true; }
inline bool   mfaLoginTokenSerial(int, char* tok) { tok[0] = 0; return true; }
inline int    mfaLoginCheck(const char*, const char*, uint32_t, int&, ErrBuf& err) { err = ""; return 0; }
inline bool   mfaLoginNoTime(const char*) { return false; }
inline void   mfaTotpAt(const char*, uint32_t, char* out) { out[0] = 0; }
#endif
