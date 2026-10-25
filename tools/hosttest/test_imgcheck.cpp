// Prova dei controlli sul file del recovery fatti da VesevOS (vos_imgcheck.h): struttura dell'immagine, marchio, versione.
// Con VOS_REC_BIN=<recovery .bin> e VOS_FW_BIN=<firmware .bin> prova anche i file veri compilati (facoltativo).
#include "mini.h"
#include <string.h>
#include <stdlib.h>
#include <vector>
#include <openssl/sha.h>
#include "../../VesevOS/src/core/vos_imgcheck.h"

// immagine finta ma corretta: 2 segmenti, somma giusta, impronta SHA-256 in fondo (come quelle di esptool)
static std::vector<uint8_t> image(bool mark, const char* ver = "0.4.4", bool hash = true) {
  std::vector<uint8_t> v(24, 0);
  v[0] = 0xE9; v[1] = 2; v[12] = 0x09; v[13] = 0x00; v[23] = hash ? 1 : 0;
  std::vector<uint8_t> s1(600, 0x11), s2(333, 0x22);
  s1[0] = 0x32; s1[1] = 0x54; s1[2] = 0xCD; s1[3] = 0xAB;               // app desc: parola magica a offset 32 del file
  if (mark) { std::string m = std::string("{REC:VesevOS:") + ver + "}"; memcpy(&s2[100], m.data(), m.size()); }
  uint8_t sum = 0xEF;
  for (auto* s : {&s1, &s2}) {
    uint8_t h[8] = {0, 0, 0, 0, (uint8_t)s->size(), (uint8_t)(s->size() >> 8), 0, 0};
    v.insert(v.end(), h, h + 8); v.insert(v.end(), s->begin(), s->end());
    for (uint8_t b : *s) sum ^= b;
  }
  while (v.size() % 16 != 15) v.push_back(0);
  v.push_back(sum);
  if (hash) { uint8_t d[32]; SHA256(v.data(), v.size(), d); v.insert(v.end(), d, d + 32); }
  return v;
}
static int chk(const std::vector<uint8_t>& v, uint32_t max = 1 << 20) { uint32_t h; return imgCheck(v.data(), (uint32_t)v.size(), max, h); }

static std::vector<uint8_t> readFile(const char* p) {
  std::vector<uint8_t> v; FILE* f = p ? fopen(p, "rb") : nullptr; if (!f) return v;
  uint8_t b[4096]; size_t n; while ((n = fread(b, 1, sizeof b, f)) > 0) v.insert(v.end(), b, b + n); fclose(f); return v;
}

int main() {
  // il marchio mascherato torna in chiaro
  char m[IMG_RMARK_LEN + 1];
  for (int i = 0; i < IMG_RMARK_LEN; i++) m[i] = (char)(IMG_RMARK_X[i] ^ 0x5A);
  m[IMG_RMARK_LEN] = 0;
  CHECK(strcmp(m, "{REC:VesevOS:") == 0);

  // immagine buona: ok, impronta trovata e giusta, versione letta
  std::vector<uint8_t> g = image(true);
  uint32_t h = 0;
  CHECK(imgCheck(g.data(), (uint32_t)g.size(), 1 << 20, h) == IMG_OK);
  CHECK(h == g.size() - 32);
  { uint8_t d[32]; SHA256(g.data(), h, d); CHECK(memcmp(d, &g[h], 32) == 0); }
  char ver[16];
  CHECK(imgRecVersion(g.data(), (uint32_t)g.size(), ver, sizeof ver) && strcmp(ver, "0.4.4") == 0);
  CHECK(chk(image(true, "0.4.4", false)) == IMG_OK);                  // anche senza impronta in fondo

  // errori di struttura
  { auto v = g; v.resize(40); CHECK(chk(v) == IMG_SMALL); }
  { auto v = g; v[0] = 0xE8; CHECK(chk(v) == IMG_MAGIC); }
  { auto v = g; v[12] = 0x00; CHECK(chk(v) == IMG_CHIP); }             // 0 = ESP32 classico
  { auto v = g; v[32] = 0; CHECK(chk(v) == IMG_APPDESC); }
  CHECK(chk(g, (uint32_t)g.size() - 1) == IMG_BIG);
  CHECK(chk(g, (uint32_t)g.size()) == IMG_OK);
  { auto v = g; v[1] = 0; CHECK(chk(v) == IMG_SEGS); }
  { auto v = g; v[1] = 17; CHECK(chk(v) == IMG_SEGS); }
  { auto v = g; v[1] = 3; CHECK(chk(v) == IMG_SHORT); }                 // un segmento in piu che non c'e
  { auto v = g; v.resize(500); CHECK(chk(v) == IMG_SHORT); }            // tagliato a meta
  { auto v = g; v[24 + 8 + 50] ^= 1; CHECK(chk(v) == IMG_SUM); }        // un byte cambiato nei dati
  { auto v = g; v.resize(v.size() - 1); CHECK(chk(v) == IMG_TAIL); }    // impronta tagliata
  { auto v = g; v.push_back(0xFF); CHECK(chk(v) == IMG_TAIL); }         // dati in piu
  { auto v = image(true, "0.4.4", false); v.push_back(0); CHECK(chk(v) == IMG_TAIL); }
  { auto v = g; v[v.size() - 1] ^= 1; CHECK(chk(v) == IMG_OK); }        // impronta sbagliata: la struttura va, l'impronta la controlla vos_recupd

  // marchio e versione
  CHECK(!imgRecVersion(image(false).data(), (uint32_t)image(false).size(), ver, sizeof ver));
  { auto v = image(false); const char* fw = "{FW:VesevOS}"; memcpy(&v[200], fw, 12); CHECK(!imgRecVersion(v.data(), (uint32_t)v.size(), ver, sizeof ver)); }   // il firmware non passa
  { std::vector<uint8_t> v = {'x', '{', '{', 'R', 'E', 'C', ':', 'V', 'e', 's', 'e', 'v', 'O', 'S', ':', '1', '.', '2', '}'};
    CHECK(imgRecVersion(v.data(), (uint32_t)v.size(), ver, sizeof ver) && strcmp(ver, "1.2") == 0); }            // "{{" prima del marchio
  { std::vector<uint8_t> v = {'{', 'R', 'E', 'C', ':', 'V', 'e', 's', 'e', 'v', 'O', 'S', ':', '}'};
    CHECK(!imgRecVersion(v.data(), (uint32_t)v.size(), ver, sizeof ver)); }                                        // versione vuota
  { std::vector<uint8_t> v = {'{', 'R', 'E', 'C', ':', 'V', 'e', 's', 'e', 'v', 'O', 'S', ':', '0', 0, '}'};
    CHECK(!imgRecVersion(v.data(), (uint32_t)v.size(), ver, sizeof ver)); }                                        // carattere non stampabile

  // lettura a pezzi (come dalla partizione): marchio a cavallo di due pezzi, resti di un recovery vecchio dopo
  for (size_t cut = 1; cut < 20; cut++) {
    auto v = image(true, "0.5.0");
    auto old = image(true, "0.4.4"); v.insert(v.end(), old.begin(), old.end());
    ImgScan s; imgScanInit(s);
    size_t pos = 24 + 8 + 600 + 8 + 100 + cut;                          // taglio dentro il marchio
    imgScanFeed(s, v.data(), pos); imgScanFeed(s, v.data() + pos, v.size() - pos);
    CHECK(s.done && strcmp(s.ver, "0.5.0") == 0);
  }
  { auto v = image(false); ImgScan s; imgScanInit(s); imgScanFeed(s, v.data(), v.size()); CHECK(!s.done); }

  // file veri (se indicati)
  std::vector<uint8_t> rb = readFile(getenv("VOS_REC_BIN")), fb = readFile(getenv("VOS_FW_BIN"));
  if (rb.size()) {
    CHECK(imgCheck(rb.data(), (uint32_t)rb.size(), 0x100000, h) == IMG_OK);
    if (h) { uint8_t d[32]; SHA256(rb.data(), h, d); CHECK(memcmp(d, &rb[h], 32) == 0); }
    CHECK(imgRecVersion(rb.data(), (uint32_t)rb.size(), ver, sizeof ver));
    printf("  recovery vero: %zu byte, versione %s\n", rb.size(), ver);
  }
  if (fb.size()) {
    CHECK(imgCheck(fb.data(), (uint32_t)fb.size(), 0x300000, h) == IMG_OK);   // struttura buona...
    CHECK(!imgRecVersion(fb.data(), (uint32_t)fb.size(), ver, sizeof ver));  // ...ma non e un recovery
    printf("  firmware vero: %zu byte, rifiutato come recovery\n", fb.size());
  }
  return done("imgcheck");
}
