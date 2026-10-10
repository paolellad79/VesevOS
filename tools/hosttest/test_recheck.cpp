// Prova dei controlli del recovery sul file del firmware (recovery_check.h).
#include "mini.h"
#include <string.h>
#include <vector>
#include "../../recovery/VesevOS_Recovery/recovery_check.h"

static std::vector<uint8_t> goodImage(size_t total, bool mark) {
  std::vector<uint8_t> v(total, 0x11);
  v[0] = 0xE9; v[12] = 0x09; v[13] = 0x00;
  v[32] = 0x32; v[33] = 0x54; v[34] = 0xCD; v[35] = 0xAB;
  if (mark) memcpy(&v[1000], "{FW:VesevOS}", 12);
  return v;
}
static RecCheck feedAll(const std::vector<uint8_t>& v, size_t chunk) {
  RecCheck c; rcInit(c);
  for (size_t i = 0; i < v.size(); i += chunk) rcFeed(c, &v[i], (i + chunk <= v.size()) ? chunk : v.size() - i);
  return c;
}

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
  CHECK(strstr(rcHeaderErr(feedAll(h, 100)), "Arduino-ESP32") != nullptr);
  auto small = goodImage(20, false);
  CHECK(strstr(rcHeaderErr(feedAll(small, 20)), "piccolo") != nullptr);

  // file vuoto
  RecCheck e; rcInit(e);
  CHECK(rcHeaderErr(e) != nullptr);
  // i messaggi non hanno apici doppi (finiscono in un JSON)
  CHECK(strchr(rcHeaderErr(e), '"') == nullptr);
  return done("recheck");
}
