// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_auth.cpp
#include "vos_auth.h"
#include "vos_config.h"
#include "vos_util.h"
#include "vos_log.h"
#include "vos_i18n.h"

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

// ---------- blocco per indirizzo IP (pagina web) ----------
// Dopo cfg.banFails password sbagliate l'IP aspetta cfg.banSecs secondi; ogni nuovo blocco raddoppia (max 1 ora).
// Limite richieste: oltre 60 richieste senza sessione valida in un minuto conta come un errore.
#define BAN_SLOTS 8
struct Ban { uint32_t ip; uint8_t fails; uint8_t level; uint32_t until; uint32_t last; uint16_t denied; uint32_t deniedT0; };
static Ban g_ban[BAN_SLOTS];

String ipToStr(uint32_t ip) {
  char b[16]; snprintf(b, sizeof(b), "%u.%u.%u.%u", (unsigned)(ip & 255), (unsigned)((ip >> 8) & 255), (unsigned)((ip >> 16) & 255), (unsigned)(ip >> 24));
  return String(b);
}

static Ban* banSlot(uint32_t ip, bool create) {
  Ban* old = &g_ban[0];
  for (int i = 0; i < BAN_SLOTS; i++) {
    if (g_ban[i].ip == ip && ip) return &g_ban[i];
    if (g_ban[i].last < old->last) old = &g_ban[i];
  }
  if (!create) return nullptr;
  for (int i = 0; i < BAN_SLOTS; i++) if (!g_ban[i].ip) { old = &g_ban[i]; break; }
  memset(old, 0, sizeof(Ban)); old->ip = ip;
  return old;
}

static bool banActive(Ban* b, uint32_t& wait) {
  if (!b || !b->until) return false;
  int32_t left = (int32_t)(b->until - millis());
  if (left <= 0) { b->until = 0; return false; }
  wait = (uint32_t)left / 1000 + 1;
  return true;
}

static void banFail(Ban* b, const char* why) {
  b->last = millis();
  uint8_t maxF = cfg.banFails < 3 ? 3 : cfg.banFails;
  if (++b->fails < maxF) { vlog("SICUREZZA: %s da %s (%u/%u)", why, ipToStr(b->ip).c_str(), b->fails, maxF); return; }
  uint32_t secs = (cfg.banSecs < 10 ? 10 : cfg.banSecs) << (b->level > 6 ? 6 : b->level);
  if (secs > 3600) secs = 3600;
  b->until = millis() + secs * 1000UL; if (!b->until) b->until = 1;
  b->fails = 0; if (b->level < 10) b->level++;
  vlog("SICUREZZA: IP %s bloccato per %lu s (blocco n. %u)", ipToStr(b->ip).c_str(), (unsigned long)secs, b->level);
}

bool authIpBlocked(uint32_t ip, uint32_t& wait) { return banActive(banSlot(ip, false), wait); }

int authCheckFrom(uint32_t ip, const String& pass, uint32_t& wait) {
  Ban* b = banSlot(ip, true);
  if (banActive(b, wait)) return 2;
  if (millis() - b->last < 1000 && b->fails) { banFail(b, "tentativi troppo veloci"); return banActive(b, wait) ? 2 : 1; }
  if (authCheck(pass)) { b->fails = 0; b->level = 0; b->last = millis(); return 0; }
  banFail(b, "password errata");
  return banActive(b, wait) ? 2 : 1;
}

void authNoteDenied(uint32_t ip) {
  Ban* b = banSlot(ip, true);
  uint32_t now = millis();
  if (now - b->deniedT0 > 60000UL) { b->deniedT0 = now; b->denied = 0; }
  if (++b->denied > 60) { b->denied = 0; b->deniedT0 = now; banFail(b, "troppe richieste senza accesso"); }
}

String authBanJson() {
  String j = "{\"fails\":" + String(cfg.banFails) + ",\"secs\":" + String((unsigned long)cfg.banSecs) + ",\"list\":[";
  bool first = true;
  for (int i = 0; i < BAN_SLOTS; i++) {
    Ban* b = &g_ban[i]; if (!b->ip || (!b->fails && !b->until && !b->level)) continue;
    uint32_t w = 0; banActive(b, w);
    if (!first) j += ","; first = false;
    j += "{\"ip\":\"" + ipToStr(b->ip) + "\",\"fails\":" + String(b->fails) + ",\"wait\":" + String((unsigned long)w) + ",\"level\":" + String(b->level) + "}";
  }
  return j + "]}";
}

String authBanText() {
  String t; int n = 0;
  for (int i = 0; i < BAN_SLOTS; i++) {
    Ban* b = &g_ban[i]; if (!b->ip || (!b->fails && !b->until && !b->level)) continue;
    uint32_t w = 0; bool on = banActive(b, w);
    t += ipToStr(b->ip) + "  " + (on ? trf("BLOCCATO ancora %lu s", (unsigned long)w) : trf("%u errori", b->fails)) + "\n"; n++;
  }
  if (!n) t = String(tr("Nessun indirizzo bloccato o sospetto")) + "\n";
  return t;
}

bool authUnban(const String& ip) {
  bool any = false;
  for (int i = 0; i < BAN_SLOTS; i++) {
    if (!g_ban[i].ip) continue;
    if (ip == "all" || ipToStr(g_ban[i].ip) == ip) { memset(&g_ban[i], 0, sizeof(Ban)); any = true; }
  }
  if (ip == "all") { g_fails = 0; g_lockUntil = 0; any = true; }
  if (any) vlog("SICUREZZA: sblocco %s", ip.c_str());
  return any;
}
