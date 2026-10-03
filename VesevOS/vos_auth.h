// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// VesevOS - vos_auth.h
// Password (hash), sessioni web, blocco dopo troppi errori.
#pragma once
#include <Arduino.h>

bool   authIsSet();                               // password gia impostata?
void   authSetPassword(const String& pass);       // salva hash (non salva su file: chiamare cfgSave)
bool   authCheck(const String& pass);             // controlla con blocco anti-forza-bruta
bool   authLocked();                              // bloccato ora?
String authNewSession();                          // crea token
bool   authSessionValid(const String& token);
void   authLogout(const String& token);
String authCookieFromHeader(const String& cookieHeader);  // estrae "vos=..."
// Shell seriale
bool   serialAuthed();
void   serialAuthSet(bool v);
