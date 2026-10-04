// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_auth.cpp
#include "vos_auth.h"
#include "vos_config.h"
#include "vos_util.h"
#include "vos_log.h"
#include "vos_i18n.h"
#include "mbedtls/md.h"

#define MAX_SESSIONS 6
#define SESSION_MS   (30UL * 60UL * 1000UL)
#define MAX_FAILS    5
#define LOCK_MS      (60UL * 1000UL)
#define HASH_ITER    3000
#define MAX_NONCES   8
#define NONCE_MS     60000UL

struct Session { String token; uint32_t last; int user; bool used; };
static Session g_sess[MAX_SESSIONS];
static uint8_t  g_fails = 0;
static uint32_t g_lockUntil = 0;
static bool g_serialAuthed = false;
static String g_secret;          // segreto di questo avvio (sale finto per i nomi che non esistono)

// Formula della password (uguale nella pagina, vedi web/index.html: hashPass)
String authHashPass(const String& salt, const String& pass) {
  String h = sha256Hex(salt + pass);
  for (int i = 0; i < HASH_ITER; i++) h = sha256Hex(h + salt + pass);
  return h;
}

static bool userOk(int i) { return i >= 0 && i < VOS_MAX_USERS && cfg.users[i].name.length() && cfg.users[i].hash.length() == 64; }

bool authIsSet() {
  for (int i = 0; i < VOS_MAX_USERS; i++) if (userOk(i) && cfg.users[i].on && cfg.users[i].role == ROLE_ADMIN) return true;
  return false;
}

const char* roleName(uint8_t r) { return r == ROLE_ADMIN ? tr("Amministratore") : r == ROLE_OPER ? tr("Operatore") : tr("Ospite"); }

int userFind(const String& name) {
  for (int i = 0; i < VOS_MAX_USERS; i++) if (cfg.users[i].name.length() && cfg.users[i].name == name) return i;
  return -1;
}

bool userNameValid(const String& n) {
  if (n.length() < 3 || n.length() > 20) return false;
  for (size_t i = 0; i < n.length(); i++) {
    char c = n[i];
    if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '.' || c == '-' || c == '_')) return false;
  }
  return true;
}

static bool passValid(const String& p, String& err) {
  if (p.length() < 6) { err = tr("Password troppo corta (min 6)"); return false; }
  if (p.length() > 64) { err = tr("Password troppo lunga (max 64)"); return false; }
  return true;
}

static int admins(int except) {
  int n = 0;
  for (int i = 0; i < VOS_MAX_USERS; i++) if (i != except && userOk(i) && cfg.users[i].on && cfg.users[i].role == ROLE_ADMIN) n++;
  return n;
}

bool userSetPassword(int idx, const String& pass, String& err) {
  if (idx < 0 || idx >= VOS_MAX_USERS || !cfg.users[idx].name.length()) { err = tr("Utente non trovato"); return false; }
  if (!passValid(pass, err)) return false;
  cfg.users[idx].salt = randomHex(16);
  cfg.users[idx].hash = authHashPass(cfg.users[idx].salt, pass);
  authLogoutUser(idx);
  return true;
}

bool userAdd(const String& name, uint8_t role, const String& pass, String& err) {
  if (!userNameValid(name)) { err = tr("Nome non valido (3-20: lettere, numeri, punto, trattino)"); return false; }
  if (userFind(name) >= 0) { err = tr("Nome gia usato"); return false; }
  if (role > ROLE_ADMIN) { err = tr("Ruolo non valido"); return false; }
  if (!passValid(pass, err)) return false;
  for (int i = 0; i < VOS_MAX_USERS; i++) {
    if (cfg.users[i].name.length()) continue;
    cfg.users[i].name = name; cfg.users[i].role = role; cfg.users[i].on = true;
    return userSetPassword(i, pass, err);
  }
  err = trf("Massimo %d utenti", VOS_MAX_USERS);
  return false;
}

bool userSet(int idx, int role, int on, String& err) {
  if (!userOk(idx)) { err = tr("Utente non trovato"); return false; }
  VosUser& u = cfg.users[idx];
  bool willAdmin = (role < 0 ? u.role : role) == ROLE_ADMIN && (on < 0 ? u.on : on);
  if (!willAdmin && u.role == ROLE_ADMIN && u.on && admins(idx) == 0) { err = tr("Serve almeno un amministratore attivo"); return false; }
  if (role >= 0) { if (role > ROLE_ADMIN) { err = tr("Ruolo non valido"); return false; } u.role = role; }
  if (on >= 0) u.on = on;
  authLogoutUser(idx);                 // ruolo o blocco cambiati: si rientra
  return true;
}

bool userDel(int idx, String& err) {
  if (idx < 0 || idx >= VOS_MAX_USERS || !cfg.users[idx].name.length()) { err = tr("Utente non trovato"); return false; }
  if (cfg.users[idx].role == ROLE_ADMIN && cfg.users[idx].on && admins(idx) == 0) { err = tr("Serve almeno un amministratore attivo"); return false; }
  authLogoutUser(idx);
  cfg.users[idx].name = ""; cfg.users[idx].salt = ""; cfg.users[idx].hash = ""; cfg.users[idx].on = false; cfg.users[idx].role = ROLE_GUEST;
  return true;
}

static bool sameHex(const String& a, const String& b) {     // confronto a tempo costante
  uint8_t d = a.length() == b.length() ? 0 : 1;
  for (size_t i = 0; i < a.length() && i < b.length(); i++) d |= a[i] ^ b[i];
  return d == 0;
}

bool userCheckPassword(int idx, const String& pass) {
  if (!userOk(idx)) return false;
  return sameHex(authHashPass(cfg.users[idx].salt, pass), cfg.users[idx].hash);
}

String usersJson() {
  String j = "[";
  bool first = true;
  for (int i = 0; i < VOS_MAX_USERS; i++) {
    const VosUser& u = cfg.users[i];
    if (!u.name.length()) continue;
    if (!first) j += ","; first = false;
    j += "{\"i\":" + String(i) + ",\"name\":\"" + jsonEscape(u.name) + "\",\"role\":" + String(u.role) + ",\"on\":" + String(u.on ? "true" : "false") +
         ",\"pass\":" + String(u.hash.length() == 64 ? "true" : "false") + "}";
  }
  return j + "]";
}

String usersText() {
  String t;
  for (int i = 0; i < VOS_MAX_USERS; i++) {
    const VosUser& u = cfg.users[i];
    if (!u.name.length()) continue;
    t += u.name + "  " + roleName(u.role) + (u.on ? "" : String("  ") + tr("(bloccato)")) + (u.hash.length() == 64 ? "" : String("  ") + tr("(senza password)")) + "\n";
  }
  if (!t.length()) t = String(tr("Nessun utente")) + "\n";
  return t;
}

// ---------- accesso: numero casuale + HMAC (la password non passa mai in rete) ----------
struct Nonce { String name, nonce; uint32_t t; bool used; };
static Nonce g_nonce[MAX_NONCES];

static String hmacHex(const String& key, const String& msg) {
  unsigned char out[32];
  const mbedtls_md_info_t* md = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
  if (!md || mbedtls_md_hmac(md, (const unsigned char*)key.c_str(), key.length(), (const unsigned char*)msg.c_str(), msg.length(), out) != 0) return "";
  static const char* hx = "0123456789abcdef";
  String r; r.reserve(64);
  for (int i = 0; i < 32; i++) { r += hx[out[i] >> 4]; r += hx[out[i] & 15]; }
  return r;
}

String authLoginStart(const String& name) {
  if (!g_secret.length()) g_secret = randomHex(16);
  int i = userFind(name);
  String salt = userOk(i) ? cfg.users[i].salt : sha256Hex(g_secret + name).substring(0, 32);   // nome sconosciuto: sale finto ma stabile
  int slot = 0; uint32_t oldest = 0xFFFFFFFF;
  for (int k = 0; k < MAX_NONCES; k++) {
    if (!g_nonce[k].used) { slot = k; break; }
    if (g_nonce[k].t < oldest) { oldest = g_nonce[k].t; slot = k; }
  }
  g_nonce[slot].used = true; g_nonce[slot].name = name; g_nonce[slot].nonce = randomHex(16); g_nonce[slot].t = millis();
  return "{\"salt\":\"" + jsonEscape(salt) + "\",\"nonce\":\"" + g_nonce[slot].nonce + "\",\"iter\":" + String(HASH_ITER) + "}";
}

// ---------- blocco per indirizzo IP ----------
// Dopo cfg.banFails errori l'IP aspetta cfg.banSecs secondi; ogni nuovo blocco raddoppia (max 1 ora).
// Limite richieste: oltre 60 richieste senza sessione valida in un minuto conta come un errore.
#define BAN_SLOTS 8
struct Ban { uint32_t ip; uint8_t fails; uint8_t level; uint32_t until; uint32_t last; uint16_t denied; uint32_t deniedT0; };
static Ban g_ban[BAN_SLOTS];

static String banIpStr(uint32_t ip) { return ipToStr(ip); }   // ordine umano (primo numero nel byte alto)

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
  if (++b->fails < maxF) { vlog("SICUREZZA: %s da %s (%u/%u)", why, banIpStr(b->ip).c_str(), b->fails, maxF); return; }
  uint32_t secs = (cfg.banSecs < 10 ? 10 : cfg.banSecs) << (b->level > 6 ? 6 : b->level);
  if (secs > 3600) secs = 3600;
  b->until = millis() + secs * 1000UL; if (!b->until) b->until = 1;
  b->fails = 0; if (b->level < 10) b->level++;
  vlog("SICUREZZA: IP %s bloccato per %lu s (blocco n. %u)", banIpStr(b->ip).c_str(), (unsigned long)secs, b->level);
}

bool authIpBlocked(uint32_t ip, uint32_t& wait) { return banActive(banSlot(ip, false), wait); }

int authLoginFinish(uint32_t ip, const String& name, const String& nonce, const String& mac, int& idx, uint32_t& wait) {
  idx = -1;
  Ban* b = banSlot(ip, true);
  if (banActive(b, wait)) return 2;
  if (millis() - b->last < 1000 && b->fails) { banFail(b, "tentativi troppo veloci"); return banActive(b, wait) ? 2 : 1; }
  // il numero casuale vale una volta sola, per quel nome e per 60 secondi
  bool found = false;
  for (int k = 0; k < MAX_NONCES; k++) {
    if (g_nonce[k].used && g_nonce[k].nonce == nonce && g_nonce[k].name == name) {
      found = millis() - g_nonce[k].t < NONCE_MS;
      g_nonce[k].used = false; g_nonce[k].nonce = "";
      break;
    }
  }
  int i = userFind(name);
  bool ok = found && userOk(i) && cfg.users[i].on && sameHex(hmacHex(cfg.users[i].hash, nonce), mac);
  if (ok && cfg.users[i].role != ROLE_ADMIN && !authIsSet()) ok = false;   // dopo il reset con BOOT entra prima l'amministratore
  if (ok) { b->fails = 0; b->level = 0; b->last = millis(); idx = i; return 0; }
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
    j += "{\"ip\":\"" + banIpStr(b->ip) + "\",\"fails\":" + String(b->fails) + ",\"wait\":" + String((unsigned long)w) + ",\"level\":" + String(b->level) + "}";
  }
  return j + "]}";
}

String authBanText() {
  String t; int n = 0;
  for (int i = 0; i < BAN_SLOTS; i++) {
    Ban* b = &g_ban[i]; if (!b->ip || (!b->fails && !b->until && !b->level)) continue;
    uint32_t w = 0; bool on = banActive(b, w);
    t += banIpStr(b->ip) + "  " + (on ? trf("BLOCCATO ancora %lu s", (unsigned long)w) : trf("%u errori", b->fails)) + "\n"; n++;
  }
  if (!n) t = String(tr("Nessun indirizzo bloccato o sospetto")) + "\n";
  return t;
}

bool authUnban(const String& ip) {
  bool any = false;
  for (int i = 0; i < BAN_SLOTS; i++) {
    if (!g_ban[i].ip) continue;
    if (ip == "all" || banIpStr(g_ban[i].ip) == ip) { memset(&g_ban[i], 0, sizeof(Ban)); any = true; }
  }
  if (ip == "all") { g_fails = 0; g_lockUntil = 0; any = true; }
  if (any) vlog("SICUREZZA: sblocco %s", ip.c_str());
  return any;
}

// ---------- sessioni ----------
String authNewSession(int user) {
  int slot = 0; uint32_t oldest = 0xFFFFFFFF;
  for (int i = 0; i < MAX_SESSIONS; i++) {
    if (!g_sess[i].used) { slot = i; break; }
    if (g_sess[i].last < oldest) { oldest = g_sess[i].last; slot = i; }
  }
  g_sess[slot].token = randomHex(16);
  g_sess[slot].last = millis();
  g_sess[slot].user = user;
  g_sess[slot].used = true;
  return g_sess[slot].token;
}

int authSessionUser(const String& token) {
  if (token.length() != 32) return -1;
  for (int i = 0; i < MAX_SESSIONS; i++) {
    if (g_sess[i].used && g_sess[i].token == token) {
      if (millis() - g_sess[i].last > SESSION_MS) { g_sess[i].used = false; return -1; }
      int u = g_sess[i].user;
      if (!userOk(u) || !cfg.users[u].on) { g_sess[i].used = false; return -1; }
      g_sess[i].last = millis();
      return u;
    }
  }
  return -1;
}

void authLogout(const String& token) {
  for (int i = 0; i < MAX_SESSIONS; i++)
    if (g_sess[i].used && g_sess[i].token == token) g_sess[i].used = false;
}

void authLogoutUser(int idx) {
  for (int i = 0; i < MAX_SESSIONS; i++) if (g_sess[i].used && g_sess[i].user == idx) g_sess[i].used = false;
}

String authCookieFromHeader(const String& c) {
  int p = c.indexOf("vos=");
  if (p < 0) return "";
  int e = c.indexOf(';', p);
  if (e < 0) e = c.length();
  return c.substring(p + 4, e);
}

// ---------- shell seriale ----------
bool authLocked() { return g_lockUntil != 0 && (int32_t)(g_lockUntil - millis()) > 0; }

bool authCheck(const String& pass) {
  if (!authIsSet() || authLocked()) return false;
  for (int i = 0; i < VOS_MAX_USERS; i++) {
    if (userOk(i) && cfg.users[i].on && cfg.users[i].role == ROLE_ADMIN && userCheckPassword(i, pass)) { g_fails = 0; g_lockUntil = 0; return true; }
  }
  if (++g_fails >= MAX_FAILS) { g_lockUntil = millis() + LOCK_MS; g_fails = 0; if (!g_lockUntil) g_lockUntil = 1; }
  return false;
}

bool serialAuthed() { return g_serialAuthed; }
void serialAuthSet(bool v) { g_serialAuthed = v; }

void authResetAdmin() {
  // tutte le password degli amministratori si azzerano: al prossimo accesso "admin" ne sceglie una nuova
  int a = userFind("admin");
  for (int i = 0; i < VOS_MAX_USERS; i++)
    if (cfg.users[i].name.length() && cfg.users[i].role == ROLE_ADMIN) { cfg.users[i].salt = ""; cfg.users[i].hash = ""; if (a < 0) a = i; }
  if (a < 0) for (int i = 0; i < VOS_MAX_USERS; i++) if (!cfg.users[i].name.length()) { a = i; cfg.users[i].name = "admin"; break; }
  if (a < 0) a = 0;
  cfg.users[a].role = ROLE_ADMIN; cfg.users[a].on = true; cfg.users[a].salt = ""; cfg.users[a].hash = "";
  for (int i = 0; i < MAX_SESSIONS; i++) g_sess[i].used = false;
  g_serialAuthed = false;
  vlog("AUTH: password degli amministratori azzerate con il tasto BOOT (primo accesso: '%s')", cfg.users[a].name.c_str());
}

// Primo accesso (nessun amministratore con password): nome dell'amministratore da creare
int authFirstAdmin() {
  int a = userFind("admin");
  if (a >= 0 && cfg.users[a].role == ROLE_ADMIN) return a;
  for (int i = 0; i < VOS_MAX_USERS; i++) if (cfg.users[i].name.length() && cfg.users[i].role == ROLE_ADMIN) return i;
  for (int i = 0; i < VOS_MAX_USERS; i++) if (!cfg.users[i].name.length()) { cfg.users[i].name = "admin"; cfg.users[i].role = ROLE_ADMIN; cfg.users[i].on = true; return i; }
  return -1;
}
