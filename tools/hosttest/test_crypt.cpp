// VesevOS - prove sul computer: backup cifrato (vos_crypt.cpp) - round trip, frase sbagliata, file manomesso
#include <Arduino.h>
#include "vos_crypt.h"
#include "vos_config.h"
#include "mini.h"

int main() {
  cfgDefaults();
  cfg.staPass = "segreto-wifi-1234"; cfg.mqttPass = "segreto-mqtt";
  String plain = cfgExport(true), sealed, back, err;
  CHECK(plain.indexOf("segreto-wifi-1234") >= 0);
  CHECK(!cryptIsSealed(plain));
  CHECK(!cryptSeal(plain, "corta", sealed, err));                    // frase troppo corta
  CHECK(cryptSeal(plain, "una frase lunga 2026", sealed, err));
  CHECK(cryptIsSealed(sealed));
  CHECK(sealed.indexOf("segreto") < 0 && sealed.indexOf("hostname") < 0);   // niente in chiaro
  printf("  testo in chiaro %u byte, cifrato %u byte\n", (unsigned)plain.length(), (unsigned)sealed.length());
  CHECK(cryptOpen(sealed, "una frase lunga 2026", back, err));
  CHECK(back == plain);
  CHECK(!cryptOpen(sealed, "una frase lunga 2027", back, err));     // frase sbagliata
  String bad = sealed; int p = bad.length() - 10; bad.setCharAt(p, bad[p] == '0' ? '1' : '0');
  CHECK(!cryptOpen(bad, "una frase lunga 2026", back, err));        // file manomesso
  CHECK(!cryptOpen("# file normale\n", "una frase lunga 2026", back, err));
  String s2; CHECK(cryptSeal(plain, "una frase lunga 2026", s2, err) && s2 != sealed);   // sale e iv casuali
  // il file cifrato si apre anche con altri programmi (OpenSSL / Python): stesso formato PBKDF2 + AES-256-GCM
  FILE* f = fopen("/tmp/vos_sealed.txt", "w"); if (f) { fputs(sealed.c_str(), f); fclose(f); }
  return done("crypt");
}
