// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_mfa.cpp
#include "vos_mfa.h"
#include "vos_common.h"
#include "vos_config.h"
#include "vos_auth.h"
#include "vos_util.h"
#include "vos_time.h"
#include "vos_log.h"
#include "vos_audit.h"
#include "vos_i18n.h"
#include "mbedtls/md.h"
#include <time.h>

static int hexNib(char c) { return c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1; }

#if VOS_WITH_MFA
#define MFA_TOK_MS   120000UL
#define MFA_TRIES    5

static String g_pend[VOS_MAX_USERS];      // segreti provvisori (non ancora confermati), solo in RAM
struct Tok { String tok; int idx; uint32_t t; uint8_t tries; bool noTime; bool recOnly; bool used; };
static Tok g_tok[2];

static String hexOfBytes(const uint8_t* b, int n) { static const char* hx = "0123456789abcdef"; String r; for (int i = 0; i < n; i++) { r += hx[b[i] >> 4]; r += hx[b[i] & 15]; } return r; }
static bool hexToBytes(const String& h, uint8_t* out, int n) {
  if ((int)h.length() != n * 2) return false;
  for (int i = 0; i < n; i++) { int a = hexNib(h[2 * i]), b = hexNib(h[2 * i + 1]); if (a < 0 || b < 0) return false; out[i] = a * 16 + b; }
  return true;
}
static String base32(const uint8_t* d, int n) {
  static const char* A = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";
  String r; int buf = 0, bits = 0;
  for (int i = 0; i < n; i++) { buf = (buf << 8) | d[i]; bits += 8; while (bits >= 5) { r += A[(buf >> (bits - 5)) & 31]; bits -= 5; } }
  if (bits) r += A[(buf << (5 - bits)) & 31];
  return r;
}

String mfaTotpAt(const String& hexSecret, uint32_t counter) {
  uint8_t key[20]; if (!hexToBytes(hexSecret, key, 20)) return "";
  uint8_t msg[8] = {0, 0, 0, 0, (uint8_t)(counter >> 24), (uint8_t)(counter >> 16), (uint8_t)(counter >> 8), (uint8_t)counter};
  uint8_t h[20];
  const mbedtls_md_info_t* md = mbedtls_md_info_from_type(MBEDTLS_MD_SHA1);
  if (!md || mbedtls_md_hmac(md, key, 20, msg, 8, h) != 0) return "";
  int o = h[19] & 15;
  uint32_t v = ((uint32_t)(h[o] & 0x7f) << 24) | ((uint32_t)h[o + 1] << 16) | ((uint32_t)h[o + 2] << 8) | h[o + 3];
  char b[8]; snprintf(b, sizeof(b), "%06lu", (unsigned long)(v % 1000000UL));
  return String(b);
}

static bool sameStr(const String& a, const String& b) {
  if (a.length() != b.length()) return false;
  uint8_t d = 0; for (size_t i = 0; i < a.length(); i++) d |= a[i] ^ b[i];
  return d == 0;
}

// controlla il codice nell'intervallo -1..+1; il contatore deve essere maggiore dell'ultimo accettato
static bool totpVerify(const String& secret, const String& code, uint32_t nowSec, uint32_t last, uint32_t& used) {
  if (code.length() != 6) return false;
  uint32_t c0 = nowSec / 30;
  for (int d = -1; d <= 1; d++) {
    uint32_t c = c0 + d;
    if (c <= last) continue;
    if (sameStr(mfaTotpAt(secret, c), code)) { used = c; return true; }
  }
  return false;
}

static bool uok(int i) { return i >= 0 && i < VOS_MAX_USERS && cfg.users[i].name.length(); }
bool mfaOn(int i) { return uok(i) && cfg.users[i].mfaOn && cfg.users[i].mfa.length() == 40; }

String mfaBegin(int idx, const String& host) {
  if (!uok(idx)) return "";
  uint8_t k[20]; esp_fill_random(k, 20);
  g_pend[idx] = hexOfBytes(k, 20);
  String b = base32(k, 20), spaced;
  for (size_t i = 0; i < b.length(); i++) { if (i && i % 4 == 0) spaced += ' '; spaced += b[i]; }
  String label = "VesevOS:" + cfg.users[idx].name + "@" + host;
  String uri = "otpauth://totp/" + label + "?secret=" + b + "&issuer=VesevOS&algorithm=SHA1&digits=6&period=30";
  return "{\"uri\":\"" + jsonEscape(uri) + "\",\"key\":\"" + spaced + "\"}";
}

static String newRecovery(int idx, String& plain) {      // 8 codici "XXXX-XXXX"; si salvano solo gli hash corti
  static const char* A = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789";
  String stored; plain = "";
  for (int n = 0; n < 8; n++) {
    uint8_t r[8]; esp_fill_random(r, 8);
    String c; for (int i = 0; i < 8; i++) c += A[r[i] & 31];
    plain += c.substring(0, 4) + "-" + c.substring(4) + (n < 7 ? " " : "");
    if (n) stored += ",";
    stored += sha256Hex("rec" + c).substring(0, 16);
  }
  return stored;
}

bool mfaConfirm(int idx, const String& code, String& recovery, String& err) {
  if (!uok(idx) || g_pend[idx].length() != 40) { err = tr("Prima premi Attiva MFA"); return false; }
  if (!timeValid()) { err = tr("Ora della scheda non impostata: imposta l'ora (Sistema > Ora) e riprova"); return false; }
  uint32_t used = 0;
  if (!totpVerify(g_pend[idx], code, (uint32_t)time(nullptr), 0, used)) { err = tr("Codice errato: controlla l'ora del telefono e riprova"); return false; }
  cfg.users[idx].mfa = g_pend[idx]; g_pend[idx] = ""; cfg.users[idx].mfaOn = true; cfg.users[idx].mfaLast = used;
  cfg.users[idx].rec = newRecovery(idx, recovery);
  vlog("SICUREZZA: MFA acceso per %s", cfg.users[idx].name.c_str());
  return true;
}

void mfaOff(int idx) {
  if (!uok(idx)) return;
  cfg.users[idx].mfa = ""; cfg.users[idx].mfaOn = false; cfg.users[idx].mfaLast = 0; cfg.users[idx].rec = ""; g_pend[idx] = "";
  vlog("SICUREZZA: MFA spento per %s", cfg.users[idx].name.c_str());
}

bool mfaNewRecovery(int idx, String& recovery) {
  if (!mfaOn(idx)) return false;
  cfg.users[idx].rec = newRecovery(idx, recovery);
  return true;
}

String mfaJson(int only) {
  String j = "{\"time\":" + String(timeValid() ? "true" : "false") + ",\"nt\":" + String(cfg.mfaNoTime) + ",\"pow\":" + String(cfg.powBits) + ",\"list\":[";
  bool first = true;
  for (int i = 0; i < VOS_MAX_USERS; i++) {
    if (!cfg.users[i].name.length() || (only >= 0 && i != only)) continue;
    if (!first) j += ","; first = false;
    int left = 0; if (mfaOn(i)) { left = cfg.users[i].rec.length() ? 1 : 0; for (size_t k = 0; k < cfg.users[i].rec.length(); k++) if (cfg.users[i].rec[k] == ',') left++; }
    j += "{\"i\":" + String(i) + ",\"name\":\"" + jsonEscape(cfg.users[i].name) + "\",\"role\":" + String(cfg.users[i].role) + ",\"on\":" + String(mfaOn(i) ? "true" : "false") + ",\"rec\":" + String(left) + "}";
  }
  return j + "]}";
}

int mfaOffAdmins() {
  int n = 0;
  for (int i = 0; i < VOS_MAX_USERS; i++)
    if (cfg.users[i].name.length() && cfg.users[i].role == ROLE_ADMIN && (cfg.users[i].mfaOn || cfg.users[i].mfa.length())) { mfaOff(i); n++; }
  if (n) auditEvent(AUD_YELLOW, "mfaboot", tr("MFA degli amministratori disattivato con il tasto BOOT"));
  return n;
}

String mfaLoginToken(int idx, bool& blocked) {
  blocked = false;
  bool nt = !timeValid();
  if (nt && cfg.mfaNoTime == 2) { blocked = true; return ""; }
  int slot = 0; uint32_t old = 0xFFFFFFFF;
  for (int k = 0; k < 2; k++) { if (!g_tok[k].used) { slot = k; break; } if (g_tok[k].t < old) { old = g_tok[k].t; slot = k; } }
  g_tok[slot].used = true; g_tok[slot].tok = randomHex(16); g_tok[slot].idx = idx; g_tok[slot].t = millis(); g_tok[slot].tries = 0;
  g_tok[slot].noTime = nt; g_tok[slot].recOnly = nt && cfg.mfaNoTime == 1;
  return g_tok[slot].tok;
}

bool mfaLoginNoTime(const String& tok) {
  for (int k = 0; k < 2; k++) if (g_tok[k].used && g_tok[k].tok == tok) return g_tok[k].noTime;
  return false;
}

static String recNorm(const String& c) { String r; for (size_t i = 0; i < c.length(); i++) { char ch = c[i]; if (ch == '-' || ch == ' ') continue; r += (char)toupper(ch); } return r; }

int mfaLoginCheck(const String& tok, const String& code, uint32_t browserNow, int& idx, String& err) {
  Tok* t = nullptr;
  for (int k = 0; k < 2; k++) if (g_tok[k].used && g_tok[k].tok == tok && tok.length()) t = &g_tok[k];
  if (!t || millis() - t->t > MFA_TOK_MS) { if (t) t->used = false; return 2; }
  idx = t->idx;
  VosUser& u = cfg.users[idx];
  String c = code; c.trim();
  bool ok = false;
  if (c.length() == 6 && !t->recOnly) {
    uint32_t used = 0;
    if (!t->noTime) ok = totpVerify(u.mfa, c, (uint32_t)time(nullptr), u.mfaLast, used);
    else if (browserNow > 1700000000UL && totpVerify(u.mfa, c, browserNow, u.mfaLast, used)) {
      ok = true;
      timeSetEpoch(browserNow);
      auditEvent(AUD_YELLOW, "mfatime", tr("Ora della scheda impostata dal browser durante l'accesso con MFA"));
      vlog("SICUREZZA: ora impostata dal browser dopo password e codice MFA corretti");
    } else if (browserNow <= 1700000000UL) { return 3; }
    if (ok) u.mfaLast = used;
  } else if (c.length() >= 8) {                                  // codice di recupero (usa e getta)
    String h = sha256Hex("rec" + recNorm(c)).substring(0, 16), out; bool hit = false;
    int from = 0;
    while (from <= (int)u.rec.length()) {
      int p = u.rec.indexOf(',', from); String one = p < 0 ? u.rec.substring(from) : u.rec.substring(from, p);
      if (one.length() && !hit && sameStr(one, h)) hit = true;
      else if (one.length()) { if (out.length()) out += ","; out += one; }
      if (p < 0) break; from = p + 1;
    }
    if (hit) { u.rec = out; ok = true; vlog("SICUREZZA: %s e entrato con un codice di recupero (ne restano piu in memoria)", u.name.c_str()); }
  }
  if (ok) { t->used = false; return 0; }
  if (++t->tries >= MFA_TRIES) { t->used = false; return 2; }
  err = "";
  return 1;
}

#endif  // VOS_WITH_MFA

bool powCheck(const String& nonce, const String& answer) {
  if (!cfg.powBits) return true;
  if (!answer.length() || answer.length() > 12) return false;
  String h = sha256Hex(nonce + ":" + answer);
  int bits = 0;
  for (size_t i = 0; i < h.length(); i++) {
    int n = hexNib(h[i]);
    if (n == 0) { bits += 4; continue; }
    if (n < 2) bits += 3; else if (n < 4) bits += 2; else if (n < 8) bits += 1;
    break;
  }
  return bits >= cfg.powBits;
}
