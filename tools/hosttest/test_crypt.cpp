// VesevOS - prove sul computer: backup cifrato (vos_crypt.cpp) - round trip, frase sbagliata, file manomesso
#include <Arduino.h>
#include "vos_crypt.h"
#include "vos_config.h"
#include "vos_heapbuf.h"
#include "mini.h"

int main() {
  cfgDefaults();
  cfg.staPass = "segreto-wifi-1234"; cfg.mqttPass = "segreto-mqtt";
  String plain = cfgExport(true), sealed, back; ErrBuf err;
  HeapBuf hs(CRYPT_OUT_MAX(plain.length())), hb(plain.length() + 16);
  auto seal = [&](const char* pass) { bool ok = cryptSeal(plain.c_str(), pass, hs, err); sealed = hs.c_str(); return ok; };
  auto open_ = [&](const String& s, const char* pass) { bool ok = cryptOpen(s.c_str(), pass, hb, err); back = hb.c_str(); return ok; };
  CHECK(plain.indexOf("segreto-wifi-1234") >= 0);
  CHECK(!cryptIsSealed(plain.c_str()));
  CHECK(!seal("corta"));                    // frase troppo corta
  CHECK(seal("una frase lunga 2026"));
  CHECK(cryptIsSealed(sealed.c_str()));
  CHECK(sealed.indexOf("segreto") < 0 && sealed.indexOf("hostname") < 0);   // niente in chiaro
  printf("  testo in chiaro %u byte, cifrato %u byte\n", (unsigned)plain.length(), (unsigned)sealed.length());
  CHECK(open_(sealed, "una frase lunga 2026"));
  CHECK(back == plain);
  CHECK(!open_(sealed, "una frase lunga 2027"));     // frase sbagliata
  String bad = sealed; int p = bad.length() - 10; bad.setCharAt(p, bad[p] == '0' ? '1' : '0');
  CHECK(!open_(bad, "una frase lunga 2026"));        // file manomesso
  CHECK(!open_("# file normale\n", "una frase lunga 2026"));
  String first = sealed; CHECK(seal("una frase lunga 2026") && sealed != first);   // sale e iv casuali
  // il file cifrato si apre anche con altri programmi (OpenSSL / Python): stesso formato PBKDF2 + AES-256-GCM
  FILE* f = fopen("/tmp/vos_sealed.txt", "w"); if (f) { fputs(sealed.c_str(), f); fclose(f); }
  return done("crypt");
}
