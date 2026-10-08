// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_crypt.cpp
// Formato (una riga): VOSENC1 <giri> <sale 16 byte> <iv 12 byte> <testo cifrato> <etichetta 16 byte>   (tutto in esadecimale)
#include "vos_crypt.h"
#include "vos_i18n.h"
#include "mbedtls/md.h"
#include "mbedtls/gcm.h"
#include <esp_random.h>

#define CRYPT_ITER 20000UL
static const char MAGIC[] = "VOSENC1";

// PBKDF2-HMAC-SHA256 (RFC 8018), una sola parte di 32 byte = chiave AES-256
static bool kdf(const String& pass, const uint8_t* salt, uint32_t iter, uint8_t* key) {
  const mbedtls_md_info_t* md = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
  if (!md || iter == 0) return false;
  const uint8_t* pw = (const uint8_t*)pass.c_str(); size_t pl = pass.length();
  uint8_t s[20], u[32], v[32], t[32];
  memcpy(s, salt, 16); s[16] = 0; s[17] = 0; s[18] = 0; s[19] = 1;
  if (mbedtls_md_hmac(md, pw, pl, s, 20, u) != 0) return false;
  memcpy(t, u, 32);
  for (uint32_t i = 1; i < iter; i++) {
    if (mbedtls_md_hmac(md, pw, pl, u, 32, v) != 0) return false;
    memcpy(u, v, 32);
    for (int j = 0; j < 32; j++) t[j] ^= u[j];
    if ((i & 1023) == 0) delay(1);                    // non blocca gli altri task
  }
  memcpy(key, t, 32);
  memset(u, 0, 32); memset(v, 0, 32); memset(t, 0, 32);
  return true;
}

static String hexOf(const uint8_t* b, size_t n) {
  static const char H[] = "0123456789abcdef";
  String s; s.reserve(n * 2);
  for (size_t i = 0; i < n; i++) { s += H[b[i] >> 4]; s += H[b[i] & 15]; }
  return s;
}
static int nib(char c) { return c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1; }
static bool unhex(const String& h, uint8_t* out, size_t n) {
  if (h.length() != n * 2) return false;
  for (size_t i = 0; i < n; i++) { int a = nib(h[2 * i]), b = nib(h[2 * i + 1]); if (a < 0 || b < 0) return false; out[i] = (uint8_t)(a * 16 + b); }
  return true;
}

// la riga cifrata inizia con "VOSENC1 " (a inizio testo o dopo un a capo): una parola uguale dentro un valore non conta
static int sealedPos(const String& t) {
  String m = String(MAGIC) + " ";
  if (t.startsWith(m)) return 0;
  int p = t.indexOf("\n" + m);
  return p < 0 ? -1 : p + 1;
}
bool cryptIsSealed(const String& text) { return sealedPos(text) >= 0; }

bool cryptSeal(const String& plain, const String& pass, String& out, String& err) {
  if (pass.length() < CRYPT_MIN_PASS) { err = trf("La frase deve avere almeno %d caratteri", CRYPT_MIN_PASS); return false; }
  uint8_t salt[16], iv[12], key[32], tag[16];
  esp_fill_random(salt, sizeof(salt)); esp_fill_random(iv, sizeof(iv));
  size_t n = plain.length();
  uint8_t* ct = (uint8_t*)malloc(n ? n : 1);
  if (!ct) { err = tr("Memoria insufficiente"); return false; }
  bool ok = kdf(pass, salt, CRYPT_ITER, key);
  if (ok) {
    mbedtls_gcm_context g; mbedtls_gcm_init(&g);
    ok = mbedtls_gcm_setkey(&g, MBEDTLS_CIPHER_ID_AES, key, 256) == 0 &&
         mbedtls_gcm_crypt_and_tag(&g, MBEDTLS_GCM_ENCRYPT, n, iv, sizeof(iv), (const uint8_t*)MAGIC, 7, (const uint8_t*)plain.c_str(), ct, sizeof(tag), tag) == 0;
    mbedtls_gcm_free(&g);
  }
  memset(key, 0, sizeof(key));
  if (!ok) { free(ct); err = tr("Cifratura non riuscita"); return false; }
  out = String(MAGIC) + " " + String((unsigned long)CRYPT_ITER) + " " + hexOf(salt, 16) + " " + hexOf(iv, 12) + " " + hexOf(ct, n) + " " + hexOf(tag, 16) + "\n";
  free(ct);
  return true;
}

bool cryptOpen(const String& sealed, const String& pass, String& plain, String& err) {
  int p = sealedPos(sealed);
  if (p < 0) { err = tr("Il file non e un backup cifrato"); return false; }
  int e = sealed.indexOf('\n', p); if (e < 0) e = sealed.length();
  String line = sealed.substring(p, e); line.trim();
  String f[6]; int k = 0, i = 0;
  while (k < 6 && i <= (int)line.length()) { int sp = line.indexOf(' ', i); if (sp < 0) sp = line.length(); f[k++] = line.substring(i, sp); i = sp + 1; }
  uint32_t iter = (uint32_t)f[1].toInt();
  if (k != 6 || iter < 1000 || iter > 200000UL || f[4].length() % 2) { err = tr("File cifrato danneggiato"); return false; }
  uint8_t salt[16], iv[12], tag[16], key[32];
  size_t n = f[4].length() / 2;
  if (!unhex(f[2], salt, 16) || !unhex(f[3], iv, 12) || !unhex(f[5], tag, 16)) { err = tr("File cifrato danneggiato"); return false; }
  uint8_t* ct = (uint8_t*)malloc(n ? n : 1); uint8_t* pt = (uint8_t*)malloc(n + 1);
  if (!ct || !pt) { free(ct); free(pt); err = tr("Memoria insufficiente"); return false; }
  bool ok = unhex(f[4], ct, n) && kdf(pass, salt, iter, key);
  if (ok) {
    mbedtls_gcm_context g; mbedtls_gcm_init(&g);
    ok = mbedtls_gcm_setkey(&g, MBEDTLS_CIPHER_ID_AES, key, 256) == 0 &&
         mbedtls_gcm_auth_decrypt(&g, n, iv, sizeof(iv), (const uint8_t*)MAGIC, 7, tag, sizeof(tag), ct, pt) == 0;
    mbedtls_gcm_free(&g);
  }
  memset(key, 0, sizeof(key));
  if (ok) { pt[n] = 0; plain = String((const char*)pt); }
  else err = tr("Frase sbagliata o file danneggiato");
  memset(pt, 0, n + 1); free(ct); free(pt);
  return ok;
}
