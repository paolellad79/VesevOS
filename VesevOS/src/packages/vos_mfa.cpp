// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_mfa.cpp
#include "vos_mfa.h"
#include "../core/vos_common.h"
#include "../core/vos_config.h"
#include "../security/vos_auth.h"
#include "../security/vos_sechash.h"
#include <ctype.h>
#include "../core/vos_time.h"
#include "../core/vos_log.h"
#include "../core/vos_audit.h"
#include "../core/vos_i18n.h"
#include "mbedtls/md.h"
#include "../drivers/vos_drv_sys.h"
#include <time.h>
#include "../drivers/vos_drv_time.h"

static int hexNib(char c) { return c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1; }

// confronto a tempo costante di due testi con la loro lunghezza
static bool sameBytes(const char* a, size_t an, const char* b, size_t bn) {
  if (an != bn) return false;
  uint8_t d = 0; for (size_t i = 0; i < an; i++) d |= (uint8_t)(a[i] ^ b[i]);
  return d == 0;
}

#if VOS_WITH_MFA
#define MFA_TOK_MS   120000UL
#define MFA_TRIES    5
#define MFA_SECRET_LEN 40                      // segreto: 20 byte in 40 cifre esadecimali
#define MFA_STORED_BUF 136                     // 8 hash corti da 16 + 7 virgole + zero

static char g_pend[VOS_MAX_USERS][MFA_SECRET_LEN + 1];      // segreti provvisori (non ancora confermati), solo in RAM
struct Tok { char tok[MFA_TOK_BUF]; int idx; uint32_t t; uint8_t tries; bool noTime; bool recOnly; bool used; };
static Tok g_tok[2];

static bool hexToBytes(const char* h, uint8_t* out, int n) {
  if (strlen(h) != (size_t)n * 2) return false;
  for (int i = 0; i < n; i++) { int a = hexNib(h[2 * i]), b = hexNib(h[2 * i + 1]); if (a < 0 || b < 0) return false; out[i] = a * 16 + b; }
  return true;
}
// 20 byte -> 32 caratteri base32 (160 bit = 32 gruppi da 5: nessun resto)
static void base32(const uint8_t* d, char out[33]) {
  static const char* A = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";
  int buf = 0, bits = 0, k = 0;
  for (int i = 0; i < 20; i++) { buf = (buf << 8) | d[i]; bits += 8; while (bits >= 5) { out[k++] = A[(buf >> (bits - 5)) & 31]; bits -= 5; } }
  out[k] = 0;
}

void mfaTotpAt(const char* hexSecret, uint32_t counter, char out[MFA_CODE_BUF]) {
  out[0] = 0;
  uint8_t key[20]; if (!hexToBytes(hexSecret, key, 20)) return;
  uint8_t msg[8] = {0, 0, 0, 0, (uint8_t)(counter >> 24), (uint8_t)(counter >> 16), (uint8_t)(counter >> 8), (uint8_t)counter};
  uint8_t h[20];
  const mbedtls_md_info_t* md = mbedtls_md_info_from_type(MBEDTLS_MD_SHA1);
  if (!md || mbedtls_md_hmac(md, key, 20, msg, 8, h) != 0) return;
  int o = h[19] & 15;
  uint32_t v = ((uint32_t)(h[o] & 0x7f) << 24) | ((uint32_t)h[o + 1] << 16) | ((uint32_t)h[o + 2] << 8) | h[o + 3];
  snprintf(out, MFA_CODE_BUF, "%06lu", (unsigned long)(v % 1000000UL));
}

// controlla il codice nell'intervallo -1..+1; il contatore deve essere maggiore dell'ultimo accettato
static bool totpVerify(const char* secret, const char* code, size_t codeLen, uint32_t nowSec, uint32_t last, uint32_t& used) {
  if (codeLen != 6) return false;
  uint32_t c0 = nowSec / 30;
  for (int d = -1; d <= 1; d++) {
    uint32_t c = c0 + d;
    if (c <= last) continue;
    char want[MFA_CODE_BUF];
    mfaTotpAt(secret, c, want);
    if (sameBytes(want, strlen(want), code, codeLen)) { used = c; return true; }
  }
  return false;
}

static bool uok(int i) { return i >= 0 && i < VOS_MAX_USERS && cfg.users[i].name.length(); }
bool mfaOn(int i) { return uok(i) && cfg.users[i].mfaOn && cfg.users[i].mfa.length() == MFA_SECRET_LEN; }

bool mfaBegin(int idx, const char* host, HeapBuf& j) {
  if (!uok(idx)) return false;
  uint8_t k[20]; drvRandomFill(k, 20);
  secHexOf(g_pend[idx], k, 20);
  char b[33];
  base32(k, b);
  const char* name = cfg.users[idx].name.c_str();
  j.add("{\"uri\":\"otpauth://totp/VesevOS:");
  j.addJson(name, strlen(name));
  j.add('@'); j.addJson(host, strlen(host));
  j.add("?secret="); j.add(b);
  j.add("&issuer=VesevOS&algorithm=SHA1&digits=6&period=30\",\"key\":\"");
  for (int i = 0; i < 32; i++) { if (i && i % 4 == 0) j.add(' '); j.add(b[i]); }
  j.add("\"}");
  return !j.oom() && !j.over();
}

// 8 codici "XXXX-XXXX" (plain, separati da spazio); in cfg.rec si salvano solo gli hash corti
static void newRecovery(int idx, char plain[MFA_REC_BUF]) {
  static const char* A = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789";
  char stored[MFA_STORED_BUF];
  size_t pn = 0, sn = 0;
  for (int n = 0; n < 8; n++) {
    uint8_t r[8]; drvRandomFill(r, 8);
    char c[9]; for (int i = 0; i < 8; i++) c[i] = A[r[i] & 31];
    c[8] = 0;
    pn += snprintf(plain + pn, MFA_REC_BUF - pn, "%.4s-%s%s", c, c + 4, n < 7 ? " " : "");
    char h[SEC_HEX_BUF]; secSha256Hex(h, {"rec", c});
    sn += snprintf(stored + sn, sizeof(stored) - sn, "%s%.16s", n ? "," : "", h);
  }
  cfg.users[idx].rec = stored;
}

bool mfaConfirm(int idx, const char* code, char recovery[MFA_REC_BUF], ErrBuf& err) {
  if (!uok(idx) || strlen(g_pend[idx]) != MFA_SECRET_LEN) { err = tr("Prima premi Attiva MFA"); return false; }
  if (!timeValid()) { err = tr("Ora della scheda non impostata: imposta l'ora (Sistema > Ora) e riprova"); return false; }
  uint32_t used = 0;
  if (!totpVerify(g_pend[idx], code, strlen(code), (uint32_t)time(nullptr), 0, used)) { err = tr("Codice errato: controlla l'ora del telefono e riprova"); return false; }
  cfg.users[idx].mfa = g_pend[idx]; g_pend[idx][0] = 0; cfg.users[idx].mfaOn = true; cfg.users[idx].mfaLast = used;
  newRecovery(idx, recovery);
  vlog("SICUREZZA: MFA acceso per %s", cfg.users[idx].name.c_str());
  return true;
}

void mfaOff(int idx) {
  if (!uok(idx)) return;
  cfg.users[idx].mfa = ""; cfg.users[idx].mfaOn = false; cfg.users[idx].mfaLast = 0; cfg.users[idx].rec = ""; g_pend[idx][0] = 0;
  vlog("SICUREZZA: MFA spento per %s", cfg.users[idx].name.c_str());
}

bool mfaNewRecovery(int idx, char recovery[MFA_REC_BUF]) {
  if (!mfaOn(idx)) return false;
  newRecovery(idx, recovery);
  return true;
}

void mfaJson(int only, HeapBuf& j) {
  j.addf("{\"time\":%s,\"nt\":%u,\"pow\":%u,\"list\":[", timeValid() ? "true" : "false", (unsigned)cfg.mfaNoTime, (unsigned)cfg.powBits);
  bool first = true;
  for (int i = 0; i < VOS_MAX_USERS; i++) {
    const VosUser& u = cfg.users[i];
    if (!u.name.length() || (only >= 0 && i != only)) continue;
    if (!first) j.add(',');
    first = false;
    int left = 0; if (mfaOn(i)) { left = u.rec.length() ? 1 : 0; for (size_t k = 0; k < u.rec.length(); k++) if (u.rec[k] == ',') left++; }
    j.addf("{\"i\":%d,\"name\":\"", i);
    j.addJson(u.name.c_str(), u.name.length());
    j.addf("\",\"role\":%u,\"on\":%s,\"rec\":%d}", (unsigned)u.role, mfaOn(i) ? "true" : "false", left);
  }
  j.add("]}");
}

int mfaOffAdmins() {
  int n = 0;
  for (int i = 0; i < VOS_MAX_USERS; i++)
    if (cfg.users[i].name.length() && cfg.users[i].role == ROLE_ADMIN && (cfg.users[i].mfaOn || cfg.users[i].mfa.length())) { mfaOff(i); n++; }
  if (n) auditEvent(AUD_YELLOW, "mfaboot", tr("MFA degli amministratori disattivato con il tasto BOOT"));
  return n;
}

// serial = true: accesso dalla seriale. La seriale non ha un browser che porti la sua ora e non e mai bloccata del tutto:
// senza ora valida accetta SOLO un codice di recupero (mai il TOTP, che senza ora non si puo verificare), qualunque sia mfaNoTime.
static bool newLoginToken(int idx, char tok[MFA_TOK_BUF], bool serial) {
  tok[0] = 0;
  bool nt = !timeValid();
  if (nt && cfg.mfaNoTime == 2 && !serial) return false;
  int slot = 0; uint32_t old = 0xFFFFFFFF;
  for (int k = 0; k < 2; k++) { if (!g_tok[k].used) { slot = k; break; } if (g_tok[k].t < old) { old = g_tok[k].t; slot = k; } }
  Tok& t = g_tok[slot];
  t.used = true; secRandomHex(t.tok, 16); t.idx = idx; t.t = drvMillis(); t.tries = 0;
  t.noTime = nt; t.recOnly = nt && (serial || cfg.mfaNoTime == 1);
  memcpy(tok, t.tok, MFA_TOK_BUF);
  return true;
}

bool mfaLoginToken(int idx, char tok[MFA_TOK_BUF]) { return newLoginToken(idx, tok, false); }
bool mfaLoginTokenSerial(int idx, char tok[MFA_TOK_BUF]) { return newLoginToken(idx, tok, true); }

bool mfaLoginNoTime(const char* tok) {
  for (int k = 0; k < 2; k++) if (g_tok[k].used && strcmp(g_tok[k].tok, tok) == 0) return g_tok[k].noTime;
  return false;
}

// Toglie da un codice di recupero i trattini e gli spazi e lo mette in maiuscolo; false se non sono esattamente 8 caratteri
static bool recNorm(const char* from, const char* to, char out[9]) {
  size_t n = 0;
  for (const char* p = from; p < to; p++) {
    if (*p == '-' || *p == ' ') continue;
    if (n == 8) return false;
    out[n++] = (char)toupper((unsigned char)*p);
  }
  out[n] = 0;
  return n == 8;
}

// Controlla un codice di recupero e, se c'e, lo toglie dall'elenco salvato (usa e getta)
static bool recUse(VosUser& u, const char* cs, const char* ce) {
  char norm[9];
  char want[SEC_HEX_BUF];
  if (recNorm(cs, ce, norm)) secSha256Hex(want, {"rec", norm}); else want[0] = 0;   // una lunghezza diversa da 8 non puo essere un codice
  want[16] = 0;
  HeapBuf out(u.rec.length() + 1);
  bool hit = false;
  const char* from = u.rec.c_str();
  while (true) {
    const char* comma = strchr(from, ',');
    size_t n = comma ? (size_t)(comma - from) : strlen(from);
    if (n && !hit && want[0] && sameBytes(from, n, want, strlen(want))) hit = true;
    else if (n) { if (out.length()) out.add(','); out.add(from, n); }
    if (!comma) break;
    from = comma + 1;
  }
  if (!hit) return false;
  u.rec = out.c_str();
  return true;
}

int mfaLoginCheck(const char* tok, const char* code, uint32_t browserNow, int& idx, ErrBuf& err) {
  Tok* t = nullptr;
  for (int k = 0; k < 2; k++) if (g_tok[k].used && tok[0] && strcmp(g_tok[k].tok, tok) == 0) t = &g_tok[k];
  if (!t || drvMillis() - t->t > MFA_TOK_MS) { if (t) t->used = false; return 2; }
  idx = t->idx;
  VosUser& u = cfg.users[idx];
  const char* cs = code; const char* ce = code + strlen(code);          // il codice senza spazi ai lati (come trim)
  while (cs < ce && isspace((unsigned char)*cs)) cs++;
  while (ce > cs && isspace((unsigned char)ce[-1])) ce--;
  size_t cl = (size_t)(ce - cs);
  bool ok = false;
  if (cl == 6 && !t->recOnly) {
    uint32_t used = 0;
    if (!t->noTime) ok = totpVerify(u.mfa.c_str(), cs, cl, (uint32_t)time(nullptr), u.mfaLast, used);
    else if (browserNow > 1700000000UL && totpVerify(u.mfa.c_str(), cs, cl, browserNow, u.mfaLast, used)) {
      ok = true;
      timeSetEpoch(browserNow);
      auditEvent(AUD_YELLOW, "mfatime", tr("Ora della scheda impostata dal browser durante l'accesso con MFA"));
      vlog("SICUREZZA: ora impostata dal browser dopo password e codice MFA corretti");
    } else if (browserNow <= 1700000000UL) { return 3; }
    if (ok) u.mfaLast = used;
  } else if (cl >= 8) {                                          // codice di recupero (usa e getta)
    if (recUse(u, cs, ce)) { ok = true; vlog("SICUREZZA: %s e entrato con un codice di recupero (ne restano piu in memoria)", u.name.c_str()); }
  }
  if (ok) { t->used = false; return 0; }
  if (++t->tries >= MFA_TRIES) { t->used = false; return 2; }
  err = "";
  return 1;
}

#endif  // VOS_WITH_MFA

bool powCheck(const char* nonce, const char* answer) {
  if (!cfg.powBits) return true;
  size_t al = strlen(answer);
  if (!al || al > 12) return false;
  char h[SEC_HEX_BUF];
  secSha256Hex(h, {nonce, ":", answer});
  int bits = 0;
  for (size_t i = 0; h[i]; i++) {
    int n = hexNib(h[i]);
    if (n == 0) { bits += 4; continue; }
    if (n < 2) bits += 3; else if (n < 4) bits += 2; else if (n < 8) bits += 1;
    break;
  }
  return bits >= cfg.powBits;
}
