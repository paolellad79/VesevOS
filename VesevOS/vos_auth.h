// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_auth.h
// Utenti (fino a 8, ruoli Amministratore / Operatore / Ospite), password con hash e sale,
// accesso senza password in chiaro (numero casuale + HMAC), sessioni, blocco dopo troppi errori.
#pragma once
#include <Arduino.h>
#include "vos_common.h"

bool   authIsSet();                               // esiste almeno un amministratore con password
String authHashPass(const String& salt, const String& pass);   // stessa formula usata dalla pagina
// ---- utenti ----
int    userFind(const String& name);              // indice o -1
bool   userNameValid(const String& name);         // 3-20: lettere, numeri, punto, trattino, sottolineato
bool   userAdd(const String& name, uint8_t role, const String& pass, String& err);
bool   userSet(int idx, int role, int on, String& err);   // -1 = non cambiare
bool   userDel(int idx, String& err);
bool   userSetPassword(int idx, const String& pass, String& err);   // non salva: chiamare cfgSave
bool   userCheckPassword(int idx, const String& pass);
String usersJson();
String usersText();
const char* roleName(uint8_t role);
// ---- accesso dalla pagina ----
// 1) authLoginStart: restituisce sale e numero casuale (anche per nomi inesistenti: non si scopre chi esiste)
String authLoginStart(const String& name);        // {"salt":"..","nonce":"..","iter":3000}
// 2) authLoginFinish: mac = HMAC-SHA256(chiave = hash della password, messaggio = nonce)
//    ritorna 0 ok (idx = utente), 1 errato, 2 IP bloccato (waitSec)
int    authLoginFinish(uint32_t ip, const String& name, const String& nonce, const String& mac, int& idx, uint32_t& waitSec);
bool   authIpBlocked(uint32_t ip, uint32_t& waitSec);
bool   authFailIp(uint32_t ip, const char* why);   // conta un errore (password, MFA, trappola); true = ora bloccato
void   authNoteDenied(uint32_t ip);               // richiesta senza sessione valida (limite richieste)
String authBanJson();
String authBanText();
bool   authUnban(const String& ip);               // "all" o un indirizzo
// ---- sessioni ----
String authNewSession(int userIdx);
int    authSessionUser(const String& token);      // indice utente o -1
void   authLogout(const String& token);
void   authLogoutUser(int idx);                   // chiude tutte le sessioni di un utente
String authCookieFromHeader(const String& cookieHeader);
// ---- shell seriale (solo amministratori) ----
bool   authCheck(const String& pass);             // una password di amministratore, con blocco anti-forza-bruta
bool   authLocked();
bool   serialAuthed();
void   serialAuthSet(bool v);
// ---- tasto BOOT 8 s: azzera la password dell'amministratore (il prossimo accesso la crea di nuovo) ----
void   authResetAdmin();
int    authFirstAdmin();                          // primo accesso: indice dell'amministratore a cui dare la password
