// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_sechash.cpp
#include "vos_sechash.h"
#include "mbedtls/md.h"
#include "mbedtls/sha256.h"
#include "../drivers/vos_drv_sys.h"

static const char HEX_DIGITS[] = "0123456789abcdef";

void secHexOf(char* out, const uint8_t* b, size_t n) {
  for (size_t i = 0; i < n; i++) { out[2 * i] = HEX_DIGITS[b[i] >> 4]; out[2 * i + 1] = HEX_DIGITS[b[i] & 15]; }
  out[2 * n] = 0;
}

void secSha256Hex(char out[SEC_HEX_BUF], std::initializer_list<SecPart> parts) {
  unsigned char h[32];
  mbedtls_sha256_context ctx;
  mbedtls_sha256_init(&ctx);
  mbedtls_sha256_starts(&ctx, 0);
  for (const SecPart& x : parts) if (x.n) mbedtls_sha256_update(&ctx, (const unsigned char*)x.p, x.n);
  mbedtls_sha256_finish(&ctx, h);
  mbedtls_sha256_free(&ctx);
  secHexOf(out, h, 32);
}

bool secHmacSha256Hex(char out[SEC_HEX_BUF], const char* key, const char* msg) {
  unsigned char h[32];
  const mbedtls_md_info_t* md = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
  if (!md || mbedtls_md_hmac(md, (const unsigned char*)key, strlen(key), (const unsigned char*)msg, strlen(msg), h) != 0) { out[0] = 0; return false; }
  secHexOf(out, h, 32);
  return true;
}

void secRandomHex(char* out, int bytes) {
  for (int i = 0; i < bytes; i++) {
    uint8_t b = drvRandom32() & 0xFF;
    out[2 * i] = HEX_DIGITS[b >> 4]; out[2 * i + 1] = HEX_DIGITS[b & 15];
  }
  out[2 * bytes] = 0;
}

bool secSameText(const char* a, const char* b) {
  size_t la = strlen(a), lb = strlen(b);
  uint8_t d = la == lb ? 0 : 1;
  for (size_t i = 0; i < la && i < lb; i++) d |= (uint8_t)(a[i] ^ b[i]);
  return d == 0;
}
