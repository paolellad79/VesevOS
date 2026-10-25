// VesevOS Recovery - piccoli aiuti (vedi rec_util.h).
#include "rec_util.h"
#include <string.h>
#include <ctype.h>
#include <esp_timer.h>
#include <esp_random.h>
#include <mbedtls/sha256.h>
#include <mbedtls/md.h>

uint32_t recMillis() { return (uint32_t)(esp_timer_get_time() / 1000); }

std::string hexOf(const uint8_t* b, size_t n) {
  static const char* hx = "0123456789abcdef";
  std::string r;
  r.reserve(n * 2);
  for (size_t i = 0; i < n; i++) { r += hx[b[i] >> 4]; r += hx[b[i] & 15]; }
  return r;
}

std::string sha256Hex(const std::string& s) {
  uint8_t out[32];
  mbedtls_sha256_context c;
  mbedtls_sha256_init(&c);
  mbedtls_sha256_starts(&c, 0);
  mbedtls_sha256_update(&c, (const uint8_t*)s.data(), s.size());
  mbedtls_sha256_finish(&c, out);
  mbedtls_sha256_free(&c);
  return hexOf(out, 32);
}

std::string hmacHex(const std::string& key, const std::string& msg) {
  uint8_t out[32];
  const mbedtls_md_info_t* md = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
  if (!md) return "";
  if (mbedtls_md_hmac(md, (const uint8_t*)key.data(), key.size(), (const uint8_t*)msg.data(), msg.size(), out) != 0) return "";
  return hexOf(out, 32);
}

std::string randomHex(int bytes) {
  uint8_t b[32];
  if (bytes > 32) bytes = 32;
  esp_fill_random(b, bytes);
  return hexOf(b, bytes);
}

bool sameStr(const std::string& a, const std::string& b) {
  if (a.size() != b.size()) return false;
  uint8_t d = 0;
  for (size_t i = 0; i < a.size(); i++) d |= (uint8_t)(a[i] ^ b[i]);
  return d == 0;
}

static int hexVal(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}

std::string urlDecode(const std::string& s) {
  std::string r;
  for (size_t i = 0; i < s.size(); i++) {
    if (s[i] == '+') r += ' ';
    else if (s[i] == '%' && i + 2 < s.size() && hexVal(s[i + 1]) >= 0 && hexVal(s[i + 2]) >= 0) {
      r += (char)(hexVal(s[i + 1]) * 16 + hexVal(s[i + 2]));
      i += 2;
    } else r += s[i];
  }
  return r;
}

std::string formGet(const std::string& body, const char* name) {
  size_t nl = strlen(name), p = 0;
  while (p <= body.size()) {
    size_t e = body.find('&', p);
    if (e == std::string::npos) e = body.size();
    if (e - p > nl && body.compare(p, nl, name) == 0 && body[p + nl] == '=') return urlDecode(body.substr(p + nl + 1, e - p - nl - 1));
    p = e + 1;
  }
  return "";
}

std::string trimStr(const std::string& s) {
  size_t a = 0, b = s.size();
  while (a < b && isspace((unsigned char)s[a])) a++;
  while (b > a && isspace((unsigned char)s[b - 1])) b--;
  return s.substr(a, b - a);
}
