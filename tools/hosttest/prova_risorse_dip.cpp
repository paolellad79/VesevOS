// SPDX-License-Identifier: GPL-3.0-or-later
// VesevOS - prova sul computer del lettore delle risorse (versione propria e dipendenza dentro il file).
// Compila le regole VERE del firmware (core/vos_risorse_carica_rule.h) senza IDF. Lo lancia prova_risorse_dip.py.
//   intest <file>                 legge l'intestazione VOSR
//   scrivi <file> <fw>            RisScrittore (file .vrs) con il firmware in uso <fw>; stampa esito e byte scritti
//   carica <elenco|-> <nome> <file> <fw> RisFileScrittore (file singolo; "-" = nessun SHA256SUMS sulla scheda); stampa anche elenco=1 se la
//                                 scheda aggiornerebbe da sola la sua riga di SHA256SUMS (file con impronta interna)
//   sost <elenco|-> <nome> <file>  risElencoSostituisci: stampa il nuovo SHA256SUMS con la riga di <nome> calcolata sul <file>
//   testo <file>                  legge le righe "# versione=" e "# dipende="
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <vector>
#include "../../VesevOS/src/core/vos_risorse_carica_rule.h"

namespace {

// SHA-256 minimo (solo per la prova)
struct Sha {
  uint32_t h[8];
  uint64_t lung;
  uint8_t blocco[64];
  size_t n;
};
const uint32_t K[64] = {
  0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
  0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
  0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
  0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2};
uint32_t ror(uint32_t x, int k) { return (x >> k) | (x << (32 - k)); }
void comprimi(Sha* s) {
  uint32_t w[64];
  for (int i = 0; i < 16; i++) w[i] = ((uint32_t)s->blocco[i*4] << 24) | ((uint32_t)s->blocco[i*4+1] << 16) | ((uint32_t)s->blocco[i*4+2] << 8) | s->blocco[i*4+3];
  for (int i = 16; i < 64; i++) {
    uint32_t a = ror(w[i-15], 7) ^ ror(w[i-15], 18) ^ (w[i-15] >> 3), b = ror(w[i-2], 17) ^ ror(w[i-2], 19) ^ (w[i-2] >> 10);
    w[i] = w[i-16] + a + w[i-7] + b;
  }
  uint32_t v[8];
  for (int i = 0; i < 8; i++) v[i] = s->h[i];
  for (int i = 0; i < 64; i++) {
    uint32_t t1 = v[7] + (ror(v[4], 6) ^ ror(v[4], 11) ^ ror(v[4], 25)) + ((v[4] & v[5]) ^ (~v[4] & v[6])) + K[i] + w[i];
    uint32_t t2 = (ror(v[0], 2) ^ ror(v[0], 13) ^ ror(v[0], 22)) + ((v[0] & v[1]) ^ (v[0] & v[2]) ^ (v[1] & v[2]));
    v[7] = v[6]; v[6] = v[5]; v[5] = v[4]; v[4] = v[3] + t1; v[3] = v[2]; v[2] = v[1]; v[1] = v[0]; v[0] = t1 + t2;
  }
  for (int i = 0; i < 8; i++) s->h[i] += v[i];
}
void shaAvvia(void* c) {
  Sha* s = (Sha*)c;
  const uint32_t i0[8] = {0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19};
  for (int i = 0; i < 8; i++) s->h[i] = i0[i];
  s->lung = 0;
  s->n = 0;
}
void shaAggiungi(void* c, const uint8_t* d, size_t n) {
  Sha* s = (Sha*)c;
  for (size_t i = 0; i < n; i++) {
    s->blocco[s->n++] = d[i];
    s->lung++;
    if (s->n == 64) { comprimi(s); s->n = 0; }
  }
}
void shaChiudi(void* c, uint8_t out[32]) {
  Sha* s = (Sha*)c;
  uint64_t bit = s->lung * 8;
  uint8_t u = 0x80;
  shaAggiungi(c, &u, 1);
  u = 0;
  while (s->n != 56) shaAggiungi(c, &u, 1);
  for (int i = 7; i >= 0; i--) { uint8_t b = (uint8_t)(bit >> (i * 8)); shaAggiungi(c, &b, 1); }
  for (int i = 0; i < 8; i++) { out[i*4] = (uint8_t)(s->h[i] >> 24); out[i*4+1] = (uint8_t)(s->h[i] >> 16); out[i*4+2] = (uint8_t)(s->h[i] >> 8); out[i*4+3] = (uint8_t)s->h[i]; }
}

struct Memoria : RisSink, RisFileSink {
  size_t scritti = 0;
  bool confermato = false;
  bool aggiorna = false;
  bool apri(const RisInfo&) override { return true; }
  bool apri(const char*, size_t) override { return true; }
  bool scrivi(const uint8_t*, size_t n) override { scritti += n; return true; }
  bool conferma() override { confermato = true; return true; }
  bool conferma(bool aggiornaElenco) override { confermato = true; aggiorna = aggiornaElenco; return true; }
  void annulla() override {}
  bool pieno() const override { return false; }
};

bool nomeNoto(const char* n, size_t l) {                  // l'elenco chiuso del firmware (risFileDaNome)
  static const char* const NOTI[] = {"comuni.vgz", "licenza-apache2.vgz", "licenza-bsd3.vgz", "licenza-mit.vgz", "manuale.txt", "radio.txt", "html.vrs"};
  for (const char* x : NOTI) if (strlen(x) == l && memcmp(x, n, l) == 0) return true;
  return false;
}

std::vector<uint8_t> leggi(const char* p) {
  std::vector<uint8_t> v;
  FILE* f = fopen(p, "rb");
  if (!f) { fprintf(stderr, "manca %s\n", p); exit(2); }
  uint8_t b[4096];
  size_t n;
  while ((n = fread(b, 1, sizeof b, f)) > 0) v.insert(v.end(), b, b + n);
  fclose(f);
  return v;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 3) return 2;
  std::string m = argv[1];
  Sha sha;
  RisSha rs = {&sha, shaAvvia, shaAggiungi, shaChiudi};
  std::vector<uint8_t> d = leggi(argv[2 + (m == "carica" ? 2 : 0) + (m == "sost" ? 2 : 0)]);
  if (m == "intest") {
    RisInfo i = {};
    RisEsito e = risIntestazione(d.data(), d.size(), RIS_TETTO_BYTE, i);
    char v[8];
    risVersioneTesto(i.verMag, i.verMin, v, sizeof v);
    printf("%s ver=%s fw=0x%08x\n", risCodice(e), v, (unsigned)i.fwMin);
  } else if (m == "scrivi" && argc == 4) {
    Memoria mem;
    RisScrittore sc(rs, mem, RIS_TETTO_BYTE, risFwImpacchetta(argv[3]));
    RisEsito e = RIS_OK;
    for (size_t o = 0; o < d.size() && e == RIS_OK; o += 100) e = sc.dai(d.data() + o, d.size() - o < 100 ? d.size() - o : 100);
    if (e == RIS_OK) e = sc.fine();
    printf("%s scritti=%zu confermato=%d\n", risCodice(e), mem.scritti, mem.confermato ? 1 : 0);
  } else if (m == "carica" && argc == 6) {
    std::vector<uint8_t> el;
    if (strcmp(argv[2], "-") != 0) el = leggi(argv[2]);
    Memoria mem;
    Sha sha2;
    RisSha rs2 = {&sha2, shaAvvia, shaAggiungi, shaChiudi};
    RisFileScrittore sc(rs, rs2, mem, risFwImpacchetta(argv[5]));
    RisEsito e = sc.iniziaFile((const char*)el.data(), el.size(), argv[3], 65536, nomeNoto);
    // pezzi di 100 byte, tranne il primo da 5 (prova delle decisioni prese a cavallo dei pezzi)
    size_t pezzo = 5;
    for (size_t o = 0; o < d.size() && e == RIS_OK; o += pezzo, pezzo = 100) e = sc.dai(d.data() + o, d.size() - o < pezzo ? d.size() - o : pezzo);
    if (e == RIS_OK) e = sc.fine();
    printf("%s scritti=%zu confermato=%d elenco=%d\n", risCodice(e), mem.scritti, mem.confermato ? 1 : 0, mem.aggiorna ? 1 : 0);
  } else if (m == "sost" && argc == 5) {
    std::vector<uint8_t> el;
    if (strcmp(argv[2], "-") != 0) el = leggi(argv[2]);
    uint8_t h[32];
    Sha s2;
    shaAvvia(&s2);
    std::vector<uint8_t> f = leggi(argv[4]);
    shaAggiungi(&s2, f.data(), f.size());
    shaChiudi(&s2, h);
    char out[RIS_ELENCO_MAX + 8];
    size_t k = risElencoSostituisci((const char*)el.data(), el.size(), argv[3], h, out, RIS_ELENCO_MAX);
    printf("%zu\n", k);
    fwrite(out, 1, k, stdout);
  } else if (m == "testo") {
    RisTesto t = risTestoLeggi((const char*)d.data(), d.size());
    char v[8];
    risVersioneTesto(t.verMag, t.verMin, v, sizeof v);
    printf("%s ver=%s fw=0x%08x\n", t.ok ? "ok" : "formato", v, (unsigned)t.fwMin);
  } else {
    return 2;
  }
  return 0;
}
