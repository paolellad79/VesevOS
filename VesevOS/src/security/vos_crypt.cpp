// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_crypt.cpp
// Formato (una riga): VOSENC1 <giri> <sale 16 byte> <iv 12 byte> <testo cifrato> <etichetta 16 byte>   (tutto in esadecimale)
#include "vos_crypt.h"
#include "../core/vos_i18n.h"
#include "vos_sechash.h"
#include <ctype.h>
#include <stdlib.h>
#include "mbedtls/md.h"
#include "mbedtls/gcm.h"
#include <esp_random.h>
#include "../drivers/vos_drv_time.h"

#define CRYPT_ITER 20000UL
static const char MAGIC[] = "VOSENC1";

// PBKDF2-HMAC-SHA256 (RFC 8018), una sola parte di 32 byte = chiave AES-256
static bool kdf(const char* pass, const uint8_t* salt, uint32_t iter, uint8_t* key) {
  const mbedtls_md_info_t* md = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
  if (!md || iter == 0) return false;
  const uint8_t* pw = (const uint8_t*)pass; size_t pl = strlen(pass);
  uint8_t s[20], u[32], v[32], t[32];
  memcpy(s, salt, 16); s[16] = 0; s[17] = 0; s[18] = 0; s[19] = 1;
  if (mbedtls_md_hmac(md, pw, pl, s, 20, u) != 0) return false;
  memcpy(t, u, 32);
  for (uint32_t i = 1; i < iter; i++) {
    if (mbedtls_md_hmac(md, pw, pl, u, 32, v) != 0) return false;
    memcpy(u, v, 32);
    for (int j = 0; j < 32; j++) t[j] ^= u[j];
    if ((i & 1023) == 0) drvDelayMs(1);                    // non blocca gli altri task
  }
  memcpy(key, t, 32);
  memset(u, 0, 32); memset(v, 0, 32); memset(t, 0, 32);
  return true;
}

static int nib(char c) { return c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1; }
static bool unhex(const char* h, size_t len, uint8_t* out, size_t n) {
  if (len != n * 2) return false;
  for (size_t i = 0; i < n; i++) { int a = nib(h[2 * i]), b = nib(h[2 * i + 1]); if (a < 0 || b < 0) return false; out[i] = (uint8_t)(a * 16 + b); }
  return true;
}

// la riga cifrata inizia con "VOSENC1 " (a inizio testo o dopo un a capo): una parola uguale dentro un valore non conta
static const char* sealedLine(const char* t) {
  static const char NL_MAGIC[] = "\nVOSENC1 ";
  if (strncmp(t, NL_MAGIC + 1, sizeof(NL_MAGIC) - 2) == 0) return t;
  const char* p = strstr(t, NL_MAGIC);
  return p ? p + 1 : nullptr;
}
bool cryptIsSealed(const char* text) { return sealedLine(text) != nullptr; }

// scrive in out i byte in esadecimale, a blocchi (niente copia intera in memoria)
static bool addHex(HeapBuf& out, const uint8_t* b, size_t n) {
  char t[65];
  for (size_t i = 0; i < n; i += 32) {
    size_t k = n - i < 32 ? n - i : 32;
    secHexOf(t, b + i, k);
    if (!out.add(t, 2 * k)) return false;
  }
  return true;
}

bool cryptSeal(const char* plain, const char* pass, HeapBuf& out, ErrBuf& err) {
  if (strlen(pass) < CRYPT_MIN_PASS) { err.setf(tr("La frase deve avere almeno %d caratteri"), CRYPT_MIN_PASS); return false; }
  uint8_t salt[16], iv[12], key[32], tag[16];
  esp_fill_random(salt, sizeof(salt)); esp_fill_random(iv, sizeof(iv));
  size_t n = strlen(plain);
  uint8_t* ct = (uint8_t*)malloc(n ? n : 1);
  if (!ct) { err = tr("Memoria insufficiente"); return false; }
  bool ok = kdf(pass, salt, CRYPT_ITER, key);
  if (ok) {
    mbedtls_gcm_context g; mbedtls_gcm_init(&g);
    ok = mbedtls_gcm_setkey(&g, MBEDTLS_CIPHER_ID_AES, key, 256) == 0 &&
         mbedtls_gcm_crypt_and_tag(&g, MBEDTLS_GCM_ENCRYPT, n, iv, sizeof(iv), (const uint8_t*)MAGIC, 7, (const uint8_t*)plain, ct, sizeof(tag), tag) == 0;
    mbedtls_gcm_free(&g);
  }
  memset(key, 0, sizeof(key));
  if (!ok) { free(ct); err = tr("Cifratura non riuscita"); return false; }
  out.clear();
  out.add(MAGIC); out.add(' '); out.addUint(CRYPT_ITER); out.add(' ');
  addHex(out, salt, 16); out.add(' ');
  addHex(out, iv, 12); out.add(' ');
  addHex(out, ct, n); out.add(' ');
  addHex(out, tag, 16); out.add('\n');
  free(ct);
  if (out.oom() || out.over()) { out.clear(); err = tr("Memoria insufficiente"); return false; }
  return true;
}

struct Field { const char* p; size_t n; };

bool cryptOpen(const char* sealed, const char* pass, HeapBuf& plain, ErrBuf& err) {
  const char* line = sealedLine(sealed);
  if (!line) { err = tr("Il file non e un backup cifrato"); return false; }
  const char* end = strchr(line, '\n'); if (!end) end = line + strlen(line);
  while (line < end && isspace((unsigned char)*line)) line++;                       // come trim()
  while (end > line && isspace((unsigned char)end[-1])) end--;
  Field f[6]; int k = 0;
  for (const char* i = line; k < 6 && i <= end; ) {                                  // sei campi separati da uno spazio
    const char* sp = (const char*)memchr(i, ' ', end - i); if (!sp) sp = end;
    f[k++] = {i, (size_t)(sp - i)};
    i = sp + 1;
  }
  char num[16] = "";
  if (k == 6) snprintf(num, sizeof(num), "%.*s", (int)f[1].n, f[1].p);
  uint32_t iter = (uint32_t)atol(num);
  if (k != 6 || iter < 1000 || iter > 200000UL || f[4].n % 2) { err = tr("File cifrato danneggiato"); return false; }
  uint8_t salt[16], iv[12], tag[16], key[32];
  size_t n = f[4].n / 2;
  if (!unhex(f[2].p, f[2].n, salt, 16) || !unhex(f[3].p, f[3].n, iv, 12) || !unhex(f[5].p, f[5].n, tag, 16)) { err = tr("File cifrato danneggiato"); return false; }
  uint8_t* ct = (uint8_t*)malloc(n ? n : 1); uint8_t* pt = (uint8_t*)malloc(n + 1);
  if (!ct || !pt) { free(ct); free(pt); err = tr("Memoria insufficiente"); return false; }
  bool ok = unhex(f[4].p, f[4].n, ct, n) && kdf(pass, salt, iter, key);
  if (ok) {
    mbedtls_gcm_context g; mbedtls_gcm_init(&g);
    ok = mbedtls_gcm_setkey(&g, MBEDTLS_CIPHER_ID_AES, key, 256) == 0 &&
         mbedtls_gcm_auth_decrypt(&g, n, iv, sizeof(iv), (const uint8_t*)MAGIC, 7, tag, sizeof(tag), ct, pt) == 0;
    mbedtls_gcm_free(&g);
  }
  memset(key, 0, sizeof(key));
  if (ok) {
    pt[n] = 0;
    plain.clear();
    if (!plain.add((const char*)pt) || plain.oom() || plain.over()) { plain.clear(); err = tr("Memoria insufficiente"); ok = false; }
  } else err = tr("Frase sbagliata o file danneggiato");
  memset(pt, 0, n + 1); free(ct); free(pt);
  return ok;
}
