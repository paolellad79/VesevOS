// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_auth.h
// Password (hash), sessioni web, blocco dopo troppi errori.
#pragma once
#include <Arduino.h>

bool   authIsSet();                               // password gia impostata?
void   authSetPassword(const String& pass);       // salva hash (non salva su file: chiamare cfgSave)
bool   authCheck(const String& pass);             // controlla con blocco anti-forza-bruta (seriale)
// Web: blocco per indirizzo IP. Ritorna 0 = ok, 1 = password errata, 2 = IP bloccato (waitSec = secondi da aspettare)
int    authCheckFrom(uint32_t ip, const String& pass, uint32_t& waitSec);
bool   authIpBlocked(uint32_t ip, uint32_t& waitSec);
void   authNoteDenied(uint32_t ip);                // richiesta senza sessione valida (limite richieste)
String authBanJson();                              // {"fails":5,"secs":60,"list":[{ip,fails,wait,level}]}
String authBanText();
bool   authUnban(const String& ip);                // "all" o un indirizzo
bool   authLocked();                              // bloccato ora?
String authNewSession();                          // crea token
bool   authSessionValid(const String& token);
void   authLogout(const String& token);
String authCookieFromHeader(const String& cookieHeader);  // estrae "vos=..."
// Shell seriale
bool   serialAuthed();
void   serialAuthSet(bool v);
