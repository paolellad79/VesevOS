// VesevOS - vos_inflate.cpp: decompressore gzip/deflate minimo, scritto per VesevOS (RFC 1951 e RFC 1952).
// Semplice e lento (un bit alla volta): per 100 KB bastano pochi centesimi di secondo. Usa poca RAM (poche centinaia di byte di stack).
// SPDX-License-Identifier: GPL-3.0-or-later OR LicenseRef-VesevOS-Commercial
#include "vos_inflate.h"

namespace {
struct Huff { uint16_t count[16]; uint16_t sym[288]; };
Huff g_lit, g_dist;            // tabelle condivise (poco stack); un solo uso alla volta (g_busy)
volatile bool g_busy = false;
struct St {
  const uint8_t* in; size_t n, pos; uint32_t bitbuf; int bitcnt;
  uint8_t* out; size_t cap, op; bool err;
};

int bits(St& s, int need) {
  uint32_t v = s.bitbuf;
  while (s.bitcnt < need) {
    if (s.pos >= s.n) { s.err = true; return 0; }
    v |= (uint32_t)s.in[s.pos++] << s.bitcnt; s.bitcnt += 8;
  }
  s.bitbuf = v >> need; s.bitcnt -= need;
  return (int)(v & ((1u << need) - 1));
}

// costruisce la tabella dalle lunghezza dei codici
void build(Huff& h, const uint8_t* len, int n) {
  for (int i = 0; i < 16; i++) h.count[i] = 0;
  for (int i = 0; i < n; i++) h.count[len[i]]++;
  uint16_t offs[16]; offs[1] = 0;
  for (int i = 1; i < 15; i++) offs[i + 1] = offs[i] + h.count[i];
  for (int i = 0; i < n; i++) if (len[i]) h.sym[offs[len[i]]++] = i;
}

int decode(St& s, const Huff& h) {
  int code = 0, first = 0, index = 0;
  for (int l = 1; l < 16; l++) {
    code |= bits(s, 1);
    if (s.err) return -1;
    int c = h.count[l];
    if (code - c < first) return h.sym[index + (code - first)];
    index += c; first += c; first <<= 1; code <<= 1;
  }
  s.err = true; return -1;
}

const uint16_t LBASE[] = {3,4,5,6,7,8,9,10,11,13,15,17,19,23,27,31,35,43,51,59,67,83,99,115,131,163,195,227,258};
const uint8_t  LEXT[]  = {0,0,0,0,0,0,0,0,1,1,1,1,2,2,2,2,3,3,3,3,4,4,4,4,5,5,5,5,0};
const uint16_t DBASE[] = {1,2,3,4,5,7,9,13,17,25,33,49,65,97,129,193,257,385,513,769,1025,1537,2049,3073,4097,6145,8193,12289,16385,24577};
const uint8_t  DEXT[]  = {0,0,0,0,1,1,2,2,3,3,4,4,5,5,6,6,7,7,8,8,9,9,10,10,11,11,12,12,13,13};

void codes(St& s, const Huff& lit, const Huff& dist) {
  for (;;) {
    int sym = decode(s, lit);
    if (s.err) return;
    if (sym < 256) {
      if (s.op >= s.cap) { s.err = true; return; }
      s.out[s.op++] = (uint8_t)sym;
    } else if (sym == 256) {
      return;
    } else {
      sym -= 257;
      if (sym >= 29) { s.err = true; return; }
      int len = LBASE[sym] + bits(s, LEXT[sym]);
      int ds = decode(s, dist);
      if (s.err || ds < 0 || ds >= 30) { s.err = true; return; }
      size_t d = DBASE[ds] + bits(s, DEXT[ds]);
      if (s.err || d > s.op || s.op + len > s.cap) { s.err = true; return; }
      while (len--) { s.out[s.op] = s.out[s.op - d]; s.op++; }
    }
  }
}

void fixedBlock(St& s) {
  Huff& lit = g_lit; Huff& dist = g_dist;
  uint8_t l[288];
  int i = 0;
  for (; i < 144; i++) l[i] = 8;
  for (; i < 256; i++) l[i] = 9;
  for (; i < 280; i++) l[i] = 7;
  for (; i < 288; i++) l[i] = 8;
  build(lit, l, 288);
  uint8_t d[30]; for (i = 0; i < 30; i++) d[i] = 5;
  build(dist, d, 30);
  codes(s, lit, dist);
}

void dynamicBlock(St& s) {
  static const uint8_t ORD[19] = {16,17,18,0,8,7,9,6,10,5,11,4,12,3,13,2,14,1,15};
  int nlen = bits(s, 5) + 257, ndist = bits(s, 5) + 1, ncode = bits(s, 4) + 4;
  if (s.err || nlen > 286 || ndist > 30) { s.err = true; return; }
  uint8_t len[320];
  int i;
  for (i = 0; i < 19; i++) len[i] = 0;
  for (i = 0; i < ncode; i++) len[ORD[i]] = (uint8_t)bits(s, 3);
  if (s.err) return;
  Huff lencode; build(lencode, len, 19);
  uint8_t ll[320]; i = 0;
  while (i < nlen + ndist) {
    int sym = decode(s, lencode);
    if (s.err) return;
    if (sym < 16) { ll[i++] = (uint8_t)sym; continue; }
    int rep, val = 0;
    if (sym == 16) { if (i == 0) { s.err = true; return; } val = ll[i - 1]; rep = 3 + bits(s, 2); }
    else if (sym == 17) rep = 3 + bits(s, 3);
    else rep = 11 + bits(s, 7);
    if (s.err || i + rep > nlen + ndist) { s.err = true; return; }
    while (rep--) ll[i++] = (uint8_t)val;
  }
  Huff& lit = g_lit; Huff& dist = g_dist;
  build(lit, ll, nlen);
  build(dist, ll + nlen, ndist);
  codes(s, lit, dist);
}
} // namespace

size_t gzipRawSize(const uint8_t* gz, size_t n) {
  if (n < 18 || gz[0] != 0x1f || gz[1] != 0x8b || gz[2] != 8) return 0;
  return (size_t)gz[n - 4] | ((size_t)gz[n - 3] << 8) | ((size_t)gz[n - 2] << 16) | ((size_t)gz[n - 1] << 24);
}

static size_t unpackImpl(const uint8_t* gz, size_t n, uint8_t* dst, size_t cap);
size_t gzipUnpack(const uint8_t* gz, size_t n, uint8_t* dst, size_t cap) {
  if (g_busy) return 0;
  g_busy = true;
  size_t r = unpackImpl(gz, n, dst, cap);
  g_busy = false;
  return r;
}

static size_t unpackImpl(const uint8_t* gz, size_t n, uint8_t* dst, size_t cap) {
  if (!gzipRawSize(gz, n)) return 0;
  size_t p = 10; uint8_t flg = gz[3];
  if (flg & 4) { if (p + 2 > n) return 0; p += 2 + (gz[p] | (gz[p + 1] << 8)); }
  if (flg & 8) { while (p < n && gz[p]) p++; p++; }
  if (flg & 16) { while (p < n && gz[p]) p++; p++; }
  if (flg & 2) p += 2;
  if (p >= n) return 0;
  St s; s.in = gz; s.n = n - 8; s.pos = p; s.bitbuf = 0; s.bitcnt = 0; s.out = dst; s.cap = cap; s.op = 0; s.err = false;
  int last;
  do {
    last = bits(s, 1);
    int type = bits(s, 2);
    if (s.err) return 0;
    if (type == 0) {
      s.bitbuf = 0; s.bitcnt = 0;
      if (s.pos + 4 > s.n) return 0;
      unsigned len = s.in[s.pos] | (s.in[s.pos + 1] << 8);
      s.pos += 4;
      if (s.pos + len > s.n || s.op + len > s.cap) return 0;
      for (unsigned i = 0; i < len; i++) s.out[s.op++] = s.in[s.pos++];
    } else if (type == 1) fixedBlock(s);
    else if (type == 2) dynamicBlock(s);
    else return 0;
    if (s.err) return 0;
  } while (!last);
  return s.op;
}
