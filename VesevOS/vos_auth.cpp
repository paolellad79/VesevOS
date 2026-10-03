// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_auth.cpp
#include "vos_auth.h"
#include "vos_config.h"
#include "vos_util.h"

#define MAX_SESSIONS 4
#define SESSION_MS   (30UL * 60UL * 1000UL)
#define MAX_FAILS    5
#define LOCK_MS      (60UL * 1000UL)

struct Session { String token; uint32_t last; bool used; };
static Session g_sess[MAX_SESSIONS];
static uint8_t  g_fails = 0;
static uint32_t g_lockUntil = 0;
static bool g_serialAuthed = false;

static String hashPass(const String& salt, const String& pass) {
  String h = sha256Hex(salt + pass);
  for (int i = 0; i < 3000; i++) h = sha256Hex(h + salt + pass);
  return h;
}

bool authIsSet() { return cfg.authHash.length() == 64 && cfg.authSalt.length() > 0; }

void authSetPassword(const String& pass) {
  cfg.authSalt = randomHex(16);
  cfg.authHash = hashPass(cfg.authSalt, pass);
  for (int i = 0; i < MAX_SESSIONS; i++) g_sess[i].used = false;  // chiude le sessioni
  g_serialAuthed = false;
}

bool authLocked() {
  return g_lockUntil != 0 && (int32_t)(g_lockUntil - millis()) > 0;
}

bool authCheck(const String& pass) {
  if (!authIsSet()) return false;
  if (authLocked()) return false;
  String h = hashPass(cfg.authSalt, pass);
  // confronto a tempo costante
  uint8_t diff = (h.length() == cfg.authHash.length()) ? 0 : 1;
  for (size_t i = 0; i < h.length() && i < cfg.authHash.length(); i++) diff |= h[i] ^ cfg.authHash[i];
  if (diff == 0) { g_fails = 0; g_lockUntil = 0; return true; }
  if (++g_fails >= MAX_FAILS) { g_lockUntil = millis() + LOCK_MS; g_fails = 0; if (!g_lockUntil) g_lockUntil = 1; }
  return false;
}

String authNewSession() {
  int slot = 0; uint32_t oldest = 0xFFFFFFFF;
  for (int i = 0; i < MAX_SESSIONS; i++) {
    if (!g_sess[i].used) { slot = i; break; }
    if (g_sess[i].last < oldest) { oldest = g_sess[i].last; slot = i; }
  }
  g_sess[slot].token = randomHex(16);
  g_sess[slot].last = millis();
  g_sess[slot].used = true;
  return g_sess[slot].token;
}

bool authSessionValid(const String& token) {
  if (token.length() != 32) return false;
  for (int i = 0; i < MAX_SESSIONS; i++) {
    if (g_sess[i].used && g_sess[i].token == token) {
      if (millis() - g_sess[i].last > SESSION_MS) { g_sess[i].used = false; return false; }
      g_sess[i].last = millis();
      return true;
    }
  }
  return false;
}

void authLogout(const String& token) {
  for (int i = 0; i < MAX_SESSIONS; i++)
    if (g_sess[i].used && g_sess[i].token == token) g_sess[i].used = false;
}

String authCookieFromHeader(const String& c) {
  int p = c.indexOf("vos=");
  if (p < 0) return "";
  int e = c.indexOf(';', p);
  if (e < 0) e = c.length();
  return c.substring(p + 4, e);
}

bool serialAuthed() { return g_serialAuthed; }
void serialAuthSet(bool v) { g_serialAuthed = v; }
