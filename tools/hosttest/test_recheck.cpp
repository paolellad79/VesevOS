// Prova dei controlli del recovery sul file del firmware (recovery_check.h).
#include "mini.h"
#include <string.h>
#include <vector>
#include "../../recovery/VesevOS_Recovery_IDF/recovery_check.h"

static std::vector<uint8_t> goodImage(size_t total, bool mark) {
  std::vector<uint8_t> v(total, 0x11);
  auto S = [&](size_t i, uint8_t b) { if (i < v.size()) v[i] = b; };      // immagini piu corte di 36 byte: solo i byte che ci stanno
  S(0, 0xE9); S(12, 0x09); S(13, 0x00);
  S(32, 0x32); S(33, 0x54); S(34, 0xCD); S(35, 0xAB);
  if (mark && v.size() >= 1012) memcpy(&v[1000], "{FW:VesevOS}", 12);
  return v;
}
static RecCheck feedAll(const std::vector<uint8_t>& v, size_t chunk) {
  RecCheck c; rcInit(c);
  for (size_t i = 0; i < v.size(); i += chunk) rcFeed(c, &v[i], (i + chunk <= v.size()) ? chunk : v.size() - i);
  return c;
}


// ricerca del marchio in app0 (rcScanMark): deve trovarlo anche dopo grandi spazi vuoti (errore del 11/10: si fermava dopo 8 KB di 0xFF)
struct Mem { const std::vector<uint8_t>* v; };
static bool memRead(void* ctx, uint32_t off, uint8_t* buf, size_t n) { const Mem* m = (const Mem*)ctx; if (off + n > m->v->size()) return false; memcpy(buf, &(*m->v)[off], n); return true; }
static bool scanImg(const std::vector<uint8_t>& v) { Mem m{&v}; uint8_t b[2048]; return rcScanMark(memRead, &m, (uint32_t)v.size(), b, sizeof b); }

int main() {
  // il marchio mascherato torna in chiaro
  char m[RC_MARK_LEN + 1];
  for (int i = 0; i < RC_MARK_LEN; i++) m[i] = (char)(RC_MARK_X[i] ^ 0x5A);
  m[RC_MARK_LEN] = 0;
  CHECK(strcmp(m, "{FW:VesevOS}") == 0);

  auto g = goodImage(5000, true);
  RecCheck c = feedAll(g, 1436);
  CHECK(rcHeaderErr(c) == nullptr);
  CHECK(rcFinalErr(c, 10000) == nullptr);
  CHECK(c.size == 5000);
  CHECK(rcFinalErr(c, 4000) != nullptr);                 // troppo grande

  // il marchio a cavallo di due pezzi si trova lo stesso, con ogni misura di pezzo
  for (size_t ch = 1; ch < 60; ch++) { RecCheck x = feedAll(g, ch); CHECK(x.mark); CHECK(rcFinalErr(x, 10000) == nullptr); }

  // senza marchio
  auto nm = goodImage(5000, false);
  CHECK(strstr(rcFinalErr(feedAll(nm, 1436), 10000), "marchio") != nullptr);

  // marchio quasi giusto (inizio ripetuto) viene comunque trovato
  auto tr = goodImage(5000, false);
  memcpy(&tr[2000], "{{FW:{FW:VesevOS}", 17);
  CHECK(feedAll(tr, 7).mark);
  auto bad = goodImage(5000, false);
  memcpy(&bad[2000], "{FW:VesevOX}", 12);
  CHECK(!feedAll(bad, 7).mark);

  // intestazioni sbagliate
  auto h = goodImage(5000, true); h[0] = 0x00;
  CHECK(strstr(rcHeaderErr(feedAll(h, 100)), "intestazione") != nullptr);
  h = goodImage(5000, true); h[12] = 0x05;               // ESP32-C3
  CHECK(strstr(rcHeaderErr(feedAll(h, 100)), "ESP32-S3") != nullptr);
  h = goodImage(5000, true); h[35] = 0x00;
  CHECK(strstr(rcHeaderErr(feedAll(h, 100)), "ESP32 valido") != nullptr);
  auto small = goodImage(20, false);
  CHECK(strstr(rcHeaderErr(feedAll(small, 20)), "piccolo") != nullptr);

  // scansione di app0
  { std::vector<uint8_t> big(2 * 1024 * 1024, 0x22);
    CHECK(!scanImg(big));                                                   // nessun marchio
    memcpy(&big[1500000], "{FW:VesevOS}", 12);                              // marchio lontano
    CHECK(scanImg(big));
    std::vector<uint8_t> gap(2 * 1024 * 1024, 0xFF);                        // spazi vuoti da 128 KB prima del marchio (il caso che falliva)
    memset(&gap[0], 0x33, 100000); memcpy(&gap[1000000], "{FW:VesevOS}", 12);
    CHECK(scanImg(gap));
    std::vector<uint8_t> edge(8192, 0x44); memcpy(&edge[2044], "{FW:VesevOS}", 12); // a cavallo di due blocchi da 2048
    CHECK(scanImg(edge));
    std::vector<uint8_t> last(8192, 0x44); memcpy(&last[8192 - 12], "{FW:VesevOS}", 12); // proprio in fondo
    CHECK(scanImg(last));
    std::vector<uint8_t> none(8192, 0xFF);
    CHECK(!scanImg(none)); }

  // file vuoto
  RecCheck e; rcInit(e);
  CHECK(rcHeaderErr(e) != nullptr);
  // i messaggi non hanno apici doppi (finiscono in un JSON)
  CHECK(strchr(rcHeaderErr(e), '"') == nullptr);
  return done("recheck");
}
