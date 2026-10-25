// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_auth.cpp
#include "vos_auth.h"
#include "../core/vos_eventbus.h"
#include "../core/vos_config.h"
#include "../core/vos_util.h"
#include "../core/vos_log.h"
#include "../core/vos_i18n.h"
#include "mbedtls/sha256.h"
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include "../drivers/vos_drv_time.h"
#include "../drivers/vos_drv_task.h"

#define MAX_SESSIONS 10
#define SESSION_MS   (30UL * 60UL * 1000UL)
#define MAX_FAILS    5
#define LOCK_MS      (60UL * 1000UL)
#define HASH_ITER    3000
#define HASH_PAUSE   250                           // ogni tanti giri l'hash lascia respirare il processore (il watchdog sul task principale)
#define MAX_NONCES   8
#define NONCE_MS     60000UL

struct Session { char token[AUTH_TOKEN_BUF]; uint32_t last; int user; bool used; uint32_t ip, t0, actT; bool secure; char act[32]; };
#define WHO_ACTIVE_MS 120000UL                     // "collegato" = ha fatto una richiesta negli ultimi 2 minuti (la pagina aperta interroga ogni pochi secondi)
static Session g_sess[MAX_SESSIONS];
// Sessioni, numeri casuali e blocchi IP sono usati INSIEME dai due server (HTTP e HTTPS sono due task) e dalla shell:
// un blocco (ricorsivo) li protegge. Creato all'avvio, prima di qualunque richiesta.
static SemaphoreHandle_t g_amx = xSemaphoreCreateRecursiveMutex();
struct AuthLock { AuthLock() { if (g_amx) xSemaphoreTakeRecursive(g_amx, portMAX_DELAY); } ~AuthLock() { if (g_amx) xSemaphoreGiveRecursive(g_amx); } };
static char g_secret[33];        // segreto di questo avvio (sale finto per i nomi che non esistono)

// Formula della password (uguale nella pagina, vedi web/index.html: hashPass)
// h0 = SHA256(sale + password); poi HASH_ITER volte h = SHA256(h in esadecimale + sale + password). Il contesto SHA si riusa a ogni giro.
void authHashPass(const char* salt, const char* pass, char out[SEC_HEX_BUF]) {
  size_t saltLen = strlen(salt), passLen = strlen(pass);
  secSha256Hex(out, {salt, pass});
  mbedtls_sha256_context ctx;
  mbedtls_sha256_init(&ctx);
  unsigned char raw[32];
  for (int i = 0; i < HASH_ITER; i++) {
    mbedtls_sha256_starts(&ctx, 0);
    mbedtls_sha256_update(&ctx, (const unsigned char*)out, 64);
    mbedtls_sha256_update(&ctx, (const unsigned char*)salt, saltLen);
    mbedtls_sha256_update(&ctx, (const unsigned char*)pass, passLen);
    mbedtls_sha256_finish(&ctx, raw);
    secHexOf(out, raw, 32);
    if ((i + 1) % HASH_PAUSE == 0) drvTaskPause();   // senza pause, piu password in fila fermavano loopTask e scattava il Task watchdog
  }
  mbedtls_sha256_free(&ctx);
  memset(raw, 0, sizeof(raw));
}

static bool userOk(int i) { return i >= 0 && i < VOS_MAX_USERS && cfg.users[i].name.length() && cfg.users[i].hash.length() == 64; }

bool authIsSet() {
  for (int i = 0; i < VOS_MAX_USERS; i++) if (userOk(i) && cfg.users[i].on && cfg.users[i].role == ROLE_ADMIN) return true;
  return false;
}

const char* roleName(uint8_t r) { return r == ROLE_ADMIN ? tr("Amministratore") : r == ROLE_OPER ? tr("Operatore") : tr("Ospite"); }

int userFind(const char* name) {
  for (int i = 0; i < VOS_MAX_USERS; i++) if (cfg.users[i].name.length() && cfg.users[i].name == name) return i;
  return -1;
}

bool userNameValid(const char* n) {
  size_t len = strlen(n);
  if (len < 3 || len > 20) return false;
  for (size_t i = 0; i < len; i++) {
    char c = n[i];
    if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '.' || c == '-' || c == '_')) return false;
  }
  return true;
}

static bool passValid(const char* p, ErrBuf& err) {
  size_t len = strlen(p);
  if (len < 6) { err = tr("Password troppo corta (min 6)"); return false; }
  if (len > 64) { err = tr("Password troppo lunga (max 64)"); return false; }
  return true;
}

static int admins(int except) {
  int n = 0;
  for (int i = 0; i < VOS_MAX_USERS; i++) if (i != except && userOk(i) && cfg.users[i].on && cfg.users[i].role == ROLE_ADMIN) n++;
  return n;
}

bool userSetPassword(int idx, const char* pass, ErrBuf& err) {
  if (idx < 0 || idx >= VOS_MAX_USERS || !cfg.users[idx].name.length()) { err = tr("Utente non trovato"); return false; }
  if (!passValid(pass, err)) return false;
  char salt[33], hash[SEC_HEX_BUF];
  secRandomHex(salt, 16);
  cfg.users[idx].salt = salt;
  authHashPass(salt, pass, hash);
  cfg.users[idx].hash = hash;
  authLogoutUser(idx);
  return true;
}

bool userAdd(const char* name, uint8_t role, const char* pass, ErrBuf& err) {
  if (!userNameValid(name)) { err = tr("Nome non valido (3-20: lettere, numeri, punto, trattino)"); return false; }
  if (userFind(name) >= 0) { err = tr("Nome gia usato"); return false; }
  if (role > ROLE_ADMIN) { err = tr("Ruolo non valido"); return false; }
  if (!passValid(pass, err)) return false;
  for (int i = 0; i < VOS_MAX_USERS; i++) {
    if (cfg.users[i].name.length()) continue;
    cfg.users[i].name = name; cfg.users[i].role = role; cfg.users[i].on = true;
    return userSetPassword(i, pass, err);
  }
  err.setf(tr("Massimo %d utenti"), VOS_MAX_USERS);
  return false;
}

bool userSet(int idx, int role, int on, ErrBuf& err) {
  if (!userOk(idx)) { err = tr("Utente non trovato"); return false; }
  VosUser& u = cfg.users[idx];
  bool willAdmin = (role < 0 ? u.role : role) == ROLE_ADMIN && (on < 0 ? u.on : on);
  if (!willAdmin && u.role == ROLE_ADMIN && u.on && admins(idx) == 0) { err = tr("Serve almeno un amministratore attivo"); return false; }
  if (role >= 0) { if (role > ROLE_ADMIN) { err = tr("Ruolo non valido"); return false; } u.role = role; }
  if (on >= 0) u.on = on;
  authLogoutUser(idx);                 // ruolo o blocco cambiati: si rientra
  return true;
}

bool userDel(int idx, ErrBuf& err) {
  if (idx < 0 || idx >= VOS_MAX_USERS || !cfg.users[idx].name.length()) { err = tr("Utente non trovato"); return false; }
  if (cfg.users[idx].role == ROLE_ADMIN && cfg.users[idx].on && admins(idx) == 0) { err = tr("Serve almeno un amministratore attivo"); return false; }
  authLogoutUser(idx);
  cfg.users[idx].name = ""; cfg.users[idx].salt = ""; cfg.users[idx].hash = ""; cfg.users[idx].on = false; cfg.users[idx].role = ROLE_GUEST;
  cfg.users[idx].mfa = ""; cfg.users[idx].mfaOn = false; cfg.users[idx].mfaLast = 0; cfg.users[idx].rec = "";
  return true;
}

bool userCheckPassword(int idx, const char* pass) {
  if (!userOk(idx)) return false;
  char h[SEC_HEX_BUF];
  authHashPass(cfg.users[idx].salt.c_str(), pass, h);
  return secSameText(h, cfg.users[idx].hash.c_str());
}

void usersJson(HeapBuf& j) {
  j.add('[');
  bool first = true;
  for (int i = 0; i < VOS_MAX_USERS; i++) {
    const VosUser& u = cfg.users[i];
    if (!u.name.length()) continue;
    if (!first) j.add(',');
    first = false;
    j.addf("{\"i\":%d,\"name\":\"", i);
    j.addJson(u.name.c_str(), u.name.length());
    j.addf("\",\"role\":%u,\"on\":%s,\"pass\":%s,\"mfa\":%s}", (unsigned)u.role, u.on ? "true" : "false",
           u.hash.length() == 64 ? "true" : "false", u.mfaOn ? "true" : "false");
  }
  j.add(']');
}

void usersText(HeapBuf& t) {
  int n = 0;
  for (int i = 0; i < VOS_MAX_USERS; i++) {
    const VosUser& u = cfg.users[i];
    if (!u.name.length()) continue;
    t.addf("%s  %s", u.name.c_str(), roleName(u.role));
    if (!u.on) t.addf("  %s", tr("(bloccato)"));
    if (u.hash.length() != 64) t.addf("  %s", tr("(senza password)"));
    t.add('\n'); n++;
  }
  if (!n) t.addf("%s\n", tr("Nessun utente"));
}

// ---------- accesso: numero casuale + HMAC (la password non passa mai in rete) ----------
struct Nonce { char name[24]; char nonce[33]; uint32_t t; bool used; };   // il nome e tagliato: i nomi veri hanno al massimo 20 caratteri
static Nonce g_nonce[MAX_NONCES];

// Il sale di un utente; per un nome che non esiste un sale finto ma stabile: la pagina e la seriale non rivelano chi esiste
void authSaltFor(const char* name, char salt[33]) {
  AuthLock L;
  if (!g_secret[0]) secRandomHex(g_secret, 16);
  int i = userFind(name);
  char fake[SEC_HEX_BUF];
  secSha256Hex(fake, {g_secret, name});
  fake[32] = 0;
  snprintf(salt, 33, "%.32s", userOk(i) ? cfg.users[i].salt.c_str() : fake);
}

// Una sola regola per web e seriale: dopo il reset con BOOT entra prima l'amministratore
bool authUserCanLogin(int i) { return userOk(i) && cfg.users[i].on && (cfg.users[i].role == ROLE_ADMIN || authIsSet()); }

void authLoginStart(const char* name, HeapBuf& j) {
  AuthLock L;
  char salt[33];
  authSaltFor(name, salt);
  int slot = 0; uint32_t oldest = 0xFFFFFFFF;
  for (int k = 0; k < MAX_NONCES; k++) {
    if (!g_nonce[k].used) { slot = k; break; }
    if (g_nonce[k].t < oldest) { oldest = g_nonce[k].t; slot = k; }
  }
  Nonce& n = g_nonce[slot];
  n.used = true; snprintf(n.name, sizeof(n.name), "%s", name); secRandomHex(n.nonce, 16); n.t = drvMillis();
  j.add("{\"salt\":\""); j.addJson(salt, strlen(salt));
  j.addf("\",\"nonce\":\"%s\",\"iter\":%d,\"pow\":%u}", n.nonce, HASH_ITER, (unsigned)cfg.powBits);
}

// Nome dell'origine di un blocco: l'indirizzo, o "seriale" per il cavo USB
struct OriginText {
  char s[16];
  explicit OriginText(uint32_t ip) { if (ip == AUTH_IP_SERIAL) snprintf(s, sizeof s, "seriale"); else ipTo(ip, s, sizeof s); }
};

// ---------- blocco per indirizzo IP ----------
// Dopo cfg.banFails errori l'IP aspetta cfg.banSecs secondi; ogni nuovo blocco raddoppia (max 1 ora).
// Limite richieste: oltre 60 richieste senza sessione valida in un minuto conta come un errore.
#define BAN_SLOTS 8
struct Ban { uint32_t ip; uint8_t fails; uint8_t level; uint32_t until; uint32_t last; uint16_t denied; uint32_t deniedT0; };
static Ban g_ban[BAN_SLOTS];

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
  int32_t left = (int32_t)(b->until - drvMillis());
  if (left <= 0) { b->until = 0; return false; }
  wait = (uint32_t)left / 1000 + 1;
  return true;
}

static void banFail(Ban* b, const char* why) {
  b->last = drvMillis();
  uint8_t maxF = cfg.banFails < 3 ? 3 : cfg.banFails;
  if (++b->fails < maxF) { vlog("SICUREZZA: %s da %s (%u/%u)", why, OriginText(b->ip).s, b->fails, maxF); return; }
  uint32_t secs = (cfg.banSecs < 10 ? 10 : cfg.banSecs) << (b->level > 6 ? 6 : b->level);
  if (secs > 3600) secs = 3600;
  b->until = drvMillis() + secs * 1000UL; if (!b->until) b->until = 1;
  b->fails = 0; if (b->level < 10) b->level++;
  vlog("SICUREZZA: origine %s bloccata per %lu s (blocco n. %u)", OriginText(b->ip).s, (unsigned long)secs, b->level);
  EventBus::publish("sec.ban", "", (int32_t)secs);     // l'indirizzo NON va nell'evento (lo vede anche l'Operatore): solo la durata
}

bool authIpBlocked(uint32_t ip, uint32_t& wait) { AuthLock L; return banActive(banSlot(ip, false), wait); }
bool authFailIp(uint32_t ip, const char* why) { AuthLock L; Ban* b = banSlot(ip, true); uint32_t w = 0; banFail(b, why); return banActive(b, w); }
void authIpOk(uint32_t ip) { AuthLock L; Ban* b = banSlot(ip, true); b->fails = 0; b->level = 0; b->last = drvMillis(); }

int authLoginFinish(uint32_t ip, const char* name, const char* nonce, const char* mac, int& idx, uint32_t& wait) {
  AuthLock L;
  idx = -1;
  Ban* b = banSlot(ip, true);
  if (banActive(b, wait)) return 2;
  if (drvMillis() - b->last < 1000 && b->fails) { banFail(b, "tentativi troppo veloci"); return banActive(b, wait) ? 2 : 1; }
  // il numero casuale vale una volta sola, per quel nome e per 60 secondi
  bool found = false;
  for (int k = 0; k < MAX_NONCES; k++) {
    if (g_nonce[k].used && strcmp(g_nonce[k].nonce, nonce) == 0 && strncmp(g_nonce[k].name, name, sizeof(g_nonce[k].name) - 1) == 0) {
      found = drvMillis() - g_nonce[k].t < NONCE_MS;
      g_nonce[k].used = false; g_nonce[k].nonce[0] = 0;
      break;
    }
  }
  int i = userFind(name);
  char want[SEC_HEX_BUF];
  bool ok = found && authUserCanLogin(i) && secHmacSha256Hex(want, cfg.users[i].hash.c_str(), nonce) && secSameText(want, mac);
  if (ok) { b->fails = 0; b->level = 0; b->last = drvMillis(); idx = i; return 0; }
  banFail(b, "password errata");
  return banActive(b, wait) ? 2 : 1;
}

void authNoteDenied(uint32_t ip) {
  AuthLock L;
  Ban* b = banSlot(ip, true);
  uint32_t now = drvMillis();
  if (now - b->deniedT0 > 60000UL) { b->deniedT0 = now; b->denied = 0; }
  if (++b->denied > 60) { b->denied = 0; b->deniedT0 = now; banFail(b, "troppe richieste senza accesso"); }
}

void authBanJson(HeapBuf& j) {
  AuthLock L;
  j.addf("{\"fails\":%u,\"secs\":%lu,\"list\":[", (unsigned)cfg.banFails, (unsigned long)cfg.banSecs);
  bool first = true;
  for (int i = 0; i < BAN_SLOTS; i++) {
    Ban* b = &g_ban[i]; if (!b->ip || (!b->fails && !b->until && !b->level)) continue;
    uint32_t w = 0; banActive(b, w);
    if (!first) j.add(',');
    first = false;
    j.addf("{\"ip\":\"%s\",\"fails\":%u,\"wait\":%lu,\"level\":%u}", OriginText(b->ip).s, (unsigned)b->fails, (unsigned long)w, (unsigned)b->level);
  }
  j.add("]}");
}

void authBanText(HeapBuf& t) {
  AuthLock L;
  int n = 0;
  for (int i = 0; i < BAN_SLOTS; i++) {
    Ban* b = &g_ban[i]; if (!b->ip || (!b->fails && !b->until && !b->level)) continue;
    uint32_t w = 0; bool on = banActive(b, w);
    t.addf("%s  ", OriginText(b->ip).s);
    if (on) t.addf(tr("BLOCCATO ancora %lu s"), (unsigned long)w); else t.addf(tr("%u errori"), (unsigned)b->fails);
    t.add('\n'); n++;
  }
  if (!n) t.addf("%s\n", tr("Nessun indirizzo bloccato o sospetto"));
}

bool authUnban(const char* ip) {
  AuthLock L;
  bool any = false, all = strcmp(ip, "all") == 0;
  for (int i = 0; i < BAN_SLOTS; i++) {
    if (!g_ban[i].ip) continue;
    if (all || strcmp(OriginText(g_ban[i].ip).s, ip) == 0) { memset(&g_ban[i], 0, sizeof(Ban)); any = true; }
  }
  if (all) any = true;
  if (any) { vlog("SICUREZZA: sblocco %s", ip); EventBus::publish("sec.unban", all ? "all" : "", 0); }
  return any;
}

// ---------- sessioni ----------
void authNewSession(char token[AUTH_TOKEN_BUF], int user, uint32_t ip, bool secure) {
  AuthLock L;
  int slot = 0; uint32_t oldest = 0xFFFFFFFF;
  for (int i = 0; i < MAX_SESSIONS; i++) {
    if (!g_sess[i].used) { slot = i; break; }
    if (g_sess[i].last < oldest) { oldest = g_sess[i].last; slot = i; }
  }
  Session& s = g_sess[slot];
  secRandomHex(s.token, 16);
  s.last = drvMillis();
  s.user = user;
  s.used = true;
  s.ip = ip; s.secure = secure; s.t0 = s.actT = s.last;
  strcpy(s.act, "login");
  memcpy(token, s.token, AUTH_TOKEN_BUF);
}

// letture che la pagina fa da sola ogni pochi secondi: non sono "azioni" dell'utente
static bool whoPoll(const char* p) {
  static const char* const P[] = {"/api/status", "/api/audit", "/api/auth", "/api/common", "/api/svc", "/api/stats"};
  for (const char* x : P) if (strcmp(p, x) == 0) return true;
  return false;
}

void authSessionNote(const char* token, uint32_t ip, bool secure, bool post, const char* path) {
  AuthLock L;
  if (strlen(token) != AUTH_TOKEN_LEN) return;
  for (int i = 0; i < MAX_SESSIONS; i++) {
    Session& s = g_sess[i];
    if (!s.used || strcmp(s.token, token) != 0) continue;
    s.ip = ip; s.secure = secure;
    if (!post && whoPoll(path)) return;
    if (strncmp(path, "/api/", 5) == 0) path += 5;
    snprintf(s.act, sizeof(s.act), "%s %s", post ? "POST" : "GET", path);
    s.actT = drvMillis();
    return;
  }
}

static bool whoLive(const Session& s) { return s.used && drvMillis() - s.last < WHO_ACTIVE_MS && userOk(s.user); }
static bool whoShown(const Session& s, uint32_t now) { return s.used && now - s.last <= SESSION_MS && userOk(s.user); }

int authSessionCount(int via) {
  AuthLock L;
  int n = 0;
  for (int i = 0; i < MAX_SESSIONS; i++) if (whoLive(g_sess[i]) && (via < 0 || (int)g_sess[i].secure == via)) n++;
  return n;
}

void authWhoJson(HeapBuf& j) {
  AuthLock L;
  j.add("{\"list\":["); bool first = true; uint32_t now = drvMillis();
  for (int i = 0; i < MAX_SESSIONS; i++) {
    const Session& s = g_sess[i];
    if (!whoShown(s, now)) continue;
    if (!first) j.add(',');
    first = false;
    const VosUser& u = cfg.users[s.user];
    j.add("{\"user\":\""); j.addJson(u.name.c_str(), u.name.length());
    j.addf("\",\"role\":%u,\"ip\":\"%s\",\"via\":\"%s\",\"live\":%s,\"age\":%lu,\"idle\":%lu,\"act\":\"", (unsigned)u.role, IpText(s.ip).s,
           s.secure ? "https" : "http", whoLive(s) ? "true" : "false", (unsigned long)((now - s.t0) / 1000UL), (unsigned long)((now - s.last) / 1000UL));
    j.addJson(s.act, strlen(s.act));
    j.addf("\",\"actAge\":%lu}", (unsigned long)((now - s.actT) / 1000UL));
  }
  j.add("]}");
}

static void whoDur(uint32_t s, char* o, size_t n) {
  if (s < 60) snprintf(o, n, "%lus", (unsigned long)s);
  else if (s < 3600) snprintf(o, n, "%lum", (unsigned long)(s / 60));
  else snprintf(o, n, "%luh%lum", (unsigned long)(s / 3600), (unsigned long)((s % 3600) / 60));
}

void authWhoText(HeapBuf& t) {
  AuthLock L;
  uint32_t now = drvMillis(); int n = 0;
  for (int i = 0; i < MAX_SESSIONS; i++) {
    const Session& s = g_sess[i];
    if (!whoShown(s, now)) continue;
    if (!n) t.addf("%s\n", tr("UTENTE     RUOLO          VIA   INDIRIZZO           DA  FERMO  ULTIMA AZIONE"));
    char since[16], idle[16], last[16];
    whoDur((now - s.t0) / 1000UL, since, sizeof(since));
    if (whoLive(s)) whoDur((now - s.last) / 1000UL, idle, sizeof(idle)); else strcpy(idle, "-");
    whoDur((now - s.actT) / 1000UL, last, sizeof(last));
    t.addf("%-10s %-14s %-5s %-15s %6s %6s  %s (%s)\n", cfg.users[s.user].name.c_str(), roleName(cfg.users[s.user].role),
           s.secure ? "https" : "http", IpText(s.ip).s, since, idle, s.act, last);
    n++;
  }
  if (!n) t.addf("%s\n", tr("Nessuno collegato dal web"));
}

int authSessionUser(const char* token) {
  AuthLock L;
  if (strlen(token) != AUTH_TOKEN_LEN) return -1;
  for (int i = 0; i < MAX_SESSIONS; i++) {
    if (g_sess[i].used && strcmp(g_sess[i].token, token) == 0) {
      if (drvMillis() - g_sess[i].last > SESSION_MS) { g_sess[i].used = false; return -1; }
      int u = g_sess[i].user;
      if (!userOk(u) || !cfg.users[u].on) { g_sess[i].used = false; return -1; }
      g_sess[i].last = drvMillis();
      return u;
    }
  }
  return -1;
}

void authLogout(const char* token) {
  AuthLock L;
  for (int i = 0; i < MAX_SESSIONS; i++)
    if (g_sess[i].used && strcmp(g_sess[i].token, token) == 0) g_sess[i].used = false;
}

void authLogoutUser(int idx) {
  AuthLock L;
  for (int i = 0; i < MAX_SESSIONS; i++) if (g_sess[i].used && g_sess[i].user == idx) g_sess[i].used = false;
  if (serialUser() == idx) serialClose();            // anche la seriale: ruolo, blocco o password cambiati, si rientra
}

void authCookieFromHeader(const char* c, char* out, size_t cap) {
  out[0] = 0;
  const char* p = strstr(c, "vos=");
  if (!p) return;
  p += 4;
  const char* e = strchr(p, ';');
  size_t n = e ? (size_t)(e - p) : strlen(p);
  if (n >= cap) return;                                // un gettone vero ha 32 caratteri: un valore piu lungo non vale
  memcpy(out, p, n);
  out[n] = 0;
}

void authResetAdmin() {
  // tutte le password degli amministratori si azzerano: al prossimo accesso "admin" ne sceglie una nuova
  int a = userFind("admin");
  for (int i = 0; i < VOS_MAX_USERS; i++)
    if (cfg.users[i].name.length() && cfg.users[i].role == ROLE_ADMIN) { cfg.users[i].salt = ""; cfg.users[i].hash = ""; if (a < 0) a = i; }
  if (a < 0) for (int i = 0; i < VOS_MAX_USERS; i++) if (!cfg.users[i].name.length()) { a = i; cfg.users[i].name = "admin"; break; }
  if (a < 0) a = 0;
  cfg.users[a].role = ROLE_ADMIN; cfg.users[a].on = true; cfg.users[a].salt = ""; cfg.users[a].hash = "";
  { AuthLock L; for (int i = 0; i < MAX_SESSIONS; i++) g_sess[i].used = false; }
  serialClose();
  cfg.serIn = true;                                  // la seriale torna ad accettare comandi
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
