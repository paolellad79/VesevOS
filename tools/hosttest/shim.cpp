// VesevOS - prove sul computer: finti pezzi della scheda (traduzione, casuali, SHA-256)
// Servono solo ai test in tools/hosttest; non vanno mai nel firmware.
#include <Arduino.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "mbedtls/sha256.h"

const char* tr(const char* it) { return it; }
String trf(const char* it, ...) {
  char b[512]; va_list a; va_start(a, it); vsnprintf(b, sizeof(b), it, a); va_end(a); return String(b);
}
static uint32_t g_seed = 12345;
uint32_t esp_random(void) { g_seed = g_seed * 1664525u + 1013904223u; return g_seed; }

// SHA-256 minimo (per provare sha256Hex senza la libreria mbedtls della scheda)
static uint32_t K[64] = {
0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2};
static uint32_t H[8]; static uint8_t buf[64]; static size_t bl; static uint64_t tl;
#define ROR(x,n) (((x)>>(n))|((x)<<(32-(n))))
static void blk(const uint8_t* p) {
  uint32_t w[64], a,b,c,d,e,f,g,h;
  for (int i = 0; i < 16; i++) w[i] = (uint32_t)p[4*i]<<24 | (uint32_t)p[4*i+1]<<16 | (uint32_t)p[4*i+2]<<8 | p[4*i+3];
  for (int i = 16; i < 64; i++) { uint32_t s0 = ROR(w[i-15],7)^ROR(w[i-15],18)^(w[i-15]>>3), s1 = ROR(w[i-2],17)^ROR(w[i-2],19)^(w[i-2]>>10); w[i] = w[i-16]+s0+w[i-7]+s1; }
  a=H[0];b=H[1];c=H[2];d=H[3];e=H[4];f=H[5];g=H[6];h=H[7];
  for (int i = 0; i < 64; i++) { uint32_t S1 = ROR(e,6)^ROR(e,11)^ROR(e,25), ch = (e&f)^(~e&g), t1 = h+S1+ch+K[i]+w[i], S0 = ROR(a,2)^ROR(a,13)^ROR(a,22), mj = (a&b)^(a&c)^(b&c), t2 = S0+mj; h=g;g=f;f=e;e=d+t1;d=c;c=b;b=a;a=t1+t2; }
  H[0]+=a;H[1]+=b;H[2]+=c;H[3]+=d;H[4]+=e;H[5]+=f;H[6]+=g;H[7]+=h;
}
extern "C" {
void mbedtls_sha256_init(mbedtls_sha256_context*) {}
void mbedtls_sha256_free(mbedtls_sha256_context*) {}
int mbedtls_sha256_starts(mbedtls_sha256_context*, int) { uint32_t i[8]={0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19}; memcpy(H,i,32); bl=0; tl=0; return 0; }
int mbedtls_sha256_update(mbedtls_sha256_context*, const unsigned char* in, size_t n) { tl += n; for (size_t i = 0; i < n; i++) { buf[bl++] = in[i]; if (bl == 64) { blk(buf); bl = 0; } } return 0; }
int mbedtls_sha256_finish(mbedtls_sha256_context*, unsigned char* out) {
  uint64_t bits = tl * 8; buf[bl++] = 0x80;
  if (bl > 56) { while (bl < 64) buf[bl++] = 0; blk(buf); bl = 0; }
  while (bl < 56) buf[bl++] = 0;
  for (int i = 0; i < 8; i++) buf[56+i] = (uint8_t)(bits >> (56 - 8*i));
  blk(buf);
  for (int i = 0; i < 8; i++) { out[4*i]=H[i]>>24; out[4*i+1]=H[i]>>16; out[4*i+2]=H[i]>>8; out[4*i+3]=H[i]; }
  return 0;
}
}

// Funzioni "non standard" che la stringa di Arduino si aspetta
#include <stdlib.h>
static char* numtoa(unsigned long long v, char* s, int radix, bool neg) {
  char t[70]; int n = 0;
  if (v == 0) t[n++] = '0';
  while (v) { int d = (int)(v % radix); t[n++] = d < 10 ? '0' + d : 'a' + d - 10; v /= radix; }
  int k = 0; if (neg) s[k++] = '-';
  while (n) s[k++] = t[--n];
  s[k] = 0; return s;
}
extern "C" {
char* itoa(int v, char* s, int r) { return v < 0 && r == 10 ? numtoa((unsigned long long)(-(long long)v), s, r, true) : numtoa((unsigned)v, s, r, false); }
char* ltoa(long v, char* s, int r) { return v < 0 && r == 10 ? numtoa((unsigned long long)(-(long long)v), s, r, true) : numtoa((unsigned long)v, s, r, false); }
char* lltoa(long long v, char* s, int r) { return v < 0 && r == 10 ? numtoa((unsigned long long)(-v), s, r, true) : numtoa((unsigned long long)v, s, r, false); }
char* ulltoa(unsigned long long v, char* s, int r) { return numtoa(v, s, r, false); }
char* utoa(unsigned v, char* s, int r) { return numtoa(v, s, r, false); }
char* ultoa(unsigned long v, char* s, int r) { return numtoa(v, s, r, false); }
char* dtostrf(double v, signed char w, unsigned char p, char* s) { snprintf(s, 64, "%*.*f", (int)w, (int)p, v); return s; }
}

// Finti pezzi del firmware che la configurazione chiama (registro, allarmi, lingue, file)
#include <LittleFS.h>
#include "vos_audit.h"
fs::LittleFSFS LittleFS;
extern "C++" {
__attribute__((weak)) void vlog(const char*, ...) {}
void diaryNote(const char*) {}
void auditRefresh() {}
void auditFix() {}
void auditDiff(const String&, const String&, const String&) {}
bool langCodeValid(const String& c) { return c.length() >= 2 && c.length() <= 8; }
}
#include <time.h>
unsigned long millis() { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return (unsigned long)(t.tv_sec * 1000UL + t.tv_nsec / 1000000UL); }
// Seriale finta: raccoglie quello che esce
HWCDC Serial;
void esp_fill_random(void* b, size_t n) { unsigned char* p = (unsigned char*)b; for (size_t i = 0; i < n; i++) p[i] = (unsigned char)(esp_random() >> 13); }
void delay(unsigned long) {}
