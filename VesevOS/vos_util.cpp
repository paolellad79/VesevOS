// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// VesevOS - vos_util.cpp
#include "vos_util.h"
#include "vos_i18n.h"
#include "mbedtls/sha256.h"
#include "esp_system.h"

bool ipParse(const String& s, uint32_t& out) {
  int parts[4]; int n = 0; int start = 0;
  String t = s; t.trim();
  if (t.length() < 7 || t.length() > 15) return false;
  for (int i = 0; i <= (int)t.length(); i++) {
    if (i == (int)t.length() || t[i] == '.') {
      if (n >= 4 || i == start) return false;
      String p = t.substring(start, i);
      for (size_t k = 0; k < p.length(); k++) if (!isDigit(p[k])) return false;
      int v = p.toInt();
      if (v < 0 || v > 255) return false;
      parts[n++] = v; start = i + 1;
    }
  }
  if (n != 4) return false;
  out = ((uint32_t)parts[0] << 24) | ((uint32_t)parts[1] << 16) | ((uint32_t)parts[2] << 8) | (uint32_t)parts[3];
  return true;
}

String ipToStr(uint32_t ip) {
  return String((ip >> 24) & 255) + "." + String((ip >> 16) & 255) + "." + String((ip >> 8) & 255) + "." + String(ip & 255);
}

bool maskValid(uint32_t m) {
  if (m == 0) return false;
  uint32_t inv = ~m;
  if ((inv & (inv + 1)) != 0) return false;     // deve essere 1111..0000
  int ones = 0;
  for (int i = 0; i < 32; i++) if (m & (1UL << i)) ones++;
  return ones >= 8 && ones <= 30;
}

String uptimeStr(uint64_t s) {
  // 1 anno = 360 giorni, 1 mese = 30 giorni
  uint64_t y = s / (360ULL * 86400); s %= (360ULL * 86400);
  uint64_t mo = s / (30ULL * 86400); s %= (30ULL * 86400);
  uint64_t d = s / 86400; s %= 86400;
  uint64_t h = s / 3600; s %= 3600;
  uint64_t mi = s / 60; s %= 60;
  // si mostrano solo le unita da quella piu grande non zero in poi
  String r;
  bool started = false;
  if (y)               { r += String((unsigned long)y) + tr("a") + " "; started = true; }
  if (started || mo)   { r += String((unsigned long)mo) + tr("m") + " "; started = true; }
  if (started || d)    { r += String((unsigned long)d) + tr("g") + " "; started = true; }
  if (started || h)    { r += String((unsigned long)h) + tr("h") + " "; started = true; }
  if (started || mi)   { r += String((unsigned long)mi) + tr("min") + " "; }
  r += String((unsigned long)s) + tr("s");
  return r;
}

String cleanAscii(const String& s) {
  String o; o.reserve(s.length());
  for (size_t i = 0; i < s.length(); i++) {
    unsigned char c = (unsigned char)s[i];
    if (c == '\r') continue;                      // a-capo di Windows: si toglie
    if (c == '\n' || c == '\t' || (c >= 32 && c < 127)) o += (char)c;
    else o += '?';
  }
  return o;
}

String jsonEscape(const String& s) {
  String o; o.reserve(s.length() + 8);
  for (size_t i = 0; i < s.length(); i++) {
    unsigned char c = (unsigned char)s[i];
    if (c == '"') o += "\\\"";
    else if (c == '\\') o += "\\\\";
    else if (c == '\n') o += "\\n";
    else if (c == '\r') o += "\\r";
    else if (c == '\t') o += "\\t";
    else if (c < 32 || c >= 127) o += '?';
    else o += (char)c;
  }
  return o;
}

String sha256Hex(const String& s) {
  unsigned char out[32];
  mbedtls_sha256_context ctx;
  mbedtls_sha256_init(&ctx);
  mbedtls_sha256_starts(&ctx, 0);
  mbedtls_sha256_update(&ctx, (const unsigned char*)s.c_str(), s.length());
  mbedtls_sha256_finish(&ctx, out);
  mbedtls_sha256_free(&ctx);
  static const char* hx = "0123456789abcdef";
  String r; r.reserve(64);
  for (int i = 0; i < 32; i++) { r += hx[out[i] >> 4]; r += hx[out[i] & 15]; }
  return r;
}

String randomHex(int bytes) {
  static const char* hx = "0123456789abcdef";
  String r; r.reserve(bytes * 2);
  for (int i = 0; i < bytes; i++) {
    uint8_t b = esp_random() & 0xFF;
    r += hx[b >> 4]; r += hx[b & 15];
  }
  return r;
}

static bool labelValid(const String& l) {
  if (l.length() < 1 || l.length() > 32) return false;
  if (l[0] == '-' || l[l.length() - 1] == '-') return false;
  for (size_t i = 0; i < l.length(); i++) {
    char c = l[i];
    if (!(isAlphaNumeric(c) || c == '-')) return false;
  }
  return true;
}

bool hostnameValid(const String& s) { return labelValid(s); }

bool domainValid(const String& s) {
  if (s.length() == 0) return true;
  if (s.length() > 60) return false;
  int start = 0;
  for (int i = 0; i <= (int)s.length(); i++) {
    if (i == (int)s.length() || s[i] == '.') {
      if (!labelValid(s.substring(start, i))) return false;
      start = i + 1;
    }
  }
  return true;
}
