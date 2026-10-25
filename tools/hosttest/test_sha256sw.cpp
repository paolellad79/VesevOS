// VesevOS - prove sul computer: SHA-256 software (core/vos_sha256_sw.cpp) contro valori noti e contro OpenSSL
#include <string.h>
#include <stdlib.h>
#include <openssl/sha.h>
#include "vos_sha256_sw.h"
#include "mini.h"

static void hex(const uint8_t* d, char* out) { for (int i = 0; i < 32; i++) sprintf(out + 2 * i, "%02x", d[i]); }

int main() {
  uint8_t out[32];
  char h[65];
  // valori noti (FIPS 180-4 e NIST)
  sha256SwHash((const uint8_t*)"abc", 3, out); hex(out, h);
  CHECK(!strcmp(h, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"));
  sha256SwHash((const uint8_t*)"", 0, out); hex(out, h);
  CHECK(!strcmp(h, "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"));
  const char* two = "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";   // 56 byte: il riempimento passa a un secondo blocco
  sha256SwHash((const uint8_t*)two, strlen(two), out); hex(out, h);
  CHECK(!strcmp(h, "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1"));
  // un milione di 'a'
  uint8_t* big = (uint8_t*)malloc(1000000); memset(big, 'a', 1000000);
  sha256SwHash(big, 1000000, out); hex(out, h);
  CHECK(!strcmp(h, "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0"));
  free(big);
  // contro OpenSSL: tutte le lunghezze da 0 a 300 (comprese quelle attorno ai limiti 55/56/63/64/119/120/128)
  uint8_t msg[300], ref[32];
  for (int i = 0; i < 300; i++) msg[i] = (uint8_t)(i * 31 + 7);
  int bad = 0;
  for (size_t n = 0; n <= 300; n++) {
    sha256SwHash(msg, n, out); SHA256(msg, n, ref);
    if (memcmp(out, ref, 32)) bad++;
  }
  CHECK(bad == 0);
  // la formula della password: stessa catena con le due implementazioni
  char a[65], b[200], c[65];
  sha256SwHash((const uint8_t*)"salepass", 8, out); hex(out, a);
  SHA256((const uint8_t*)"salepass", 8, ref); hex(ref, c);
  CHECK(!strcmp(a, c));
  for (int i = 0; i < 3000; i++) {
    memcpy(b, a, 64); memcpy(b + 64, "salepass", 8);
    sha256SwHash((const uint8_t*)b, 72, out); hex(out, a);
    memcpy(b, c, 64); memcpy(b + 64, "salepass", 8);
    SHA256((const uint8_t*)b, 72, ref); hex(ref, c);
  }
  CHECK(!strcmp(a, c));
  return done("sha256sw");
}
