// Prova sul computer delle risorse dei driver ELF (devices/vos_drvres.cpp, VERO) con finti: registro dei pin, periferiche, file system in memoria.
// Prova: pin dati al driver (vietati, gia usati, doppi), conteggio per bus, nome gia usato da una periferica, bus I2C del sensore a classi,
// salvataggio e rilettura (anche con una riga rovinata o con un pin occupato), rimozione che libera i pin.
#include <stdio.h>
#include <string.h>
#include <map>
#include <string>
#include "vos_drvres.h"
#include "mini.h"

// ---- finti ----
const char* tr(const char* it) { return it; }
void vlog(const char*, ...) {}
bool utilNameOk(const char* s, size_t n) {
  if (n < 1 || n > 24) return false;
  for (size_t i = 0; i < n; i++) { char c = s[i]; if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_')) return false; }
  return true;
}
static std::map<int, std::string> g_pin;                    // pin -> proprietario
static std::map<std::string, std::string> g_fs;
static bool g_sensore = false, g_scrivi = true;
const char* pinTestBlock(int g) {
  if (g < 0 || g > 48) return "Pin inesistente su questa scheda";
  if (g == 19 || g == 20) return "USB";
  if (g == 0) return "Pin di avvio";
  if (g_pin.count(g)) return "Gia usato dal sistema";
  return nullptr;
}
bool pinClaim(uint8_t g, const char* owner, const char*, bool, const char*) { if (g_pin.count(g)) return false; g_pin[g] = owner; return true; }
void pinRelease(uint8_t g) { g_pin.erase(g); }
bool ioHas(const char* n) { return !strcmp(n, "rele1"); }
bool ioHasI2c() { return g_sensore; }
size_t fsTextMax() { return 8192; }
bool fsReadText(std::string_view p, HeapBuf& out, ErrBuf&) { auto i = g_fs.find(std::string(p)); if (i == g_fs.end()) return false; out.add(i->second.c_str()); return true; }
bool fsWriteText(std::string_view p, std::string_view t, ErrBuf& e, bool) { if (!g_scrivi) { e = "no"; return false; } g_fs[std::string(p)] = std::string(t); return true; }
bool drvFsExists(const char* p) { return g_fs.count(p) > 0; }
bool drvFsRemove(const char* p) { return g_fs.erase(p) > 0; }

int main() {
  ErrBuf e; DrvRes r;
  int i2c[] = { 21, 22 }, spi[] = { 10, 11, 12, 13 }, gp[] = { 5, 6, 7 };
  CHECK(drvresCount() == 0 && !drvresFind("bmp", r));
  // I2C
  CHECK(drvresAdd("bmp", DRB_I2C, i2c, 2, 0x76, 100, e) && drvresCount() == 1);
  CHECK(drvresFind("bmp", r) && r.bus == DRB_I2C && r.pin[0] == 21 && r.pin[1] == 22 && r.addr == 0x76 && r.khz == 100 && !strcmp(drvresBusName(r.bus), "i2c"));
  CHECK(g_pin[21] == "bmp" && g_pin[22] == "bmp" && g_fs.count("/drvres.cfg") == 1);
  CHECK(!drvresAdd("bmp", DRB_I2C, i2c, 2, 0x76, 100, e) && e.length());                 // stesso nome
  int altri[] = { 21, 23 };
  CHECK(!drvresAdd("altro", DRB_I2C, altri, 2, 0x76, 100, e) && !strcmp(e.c_str(), "Gia usato dal sistema") && !g_pin.count(23));   // pin gia dato: niente resta preso
  int doppi[] = { 30, 30 };
  CHECK(!drvresAdd("altro", DRB_I2C, doppi, 2, 0x76, 100, e) && !strcmp(e.c_str(), "Pin ripetuto"));
  int usb[] = { 19, 30 };
  CHECK(!drvresAdd("altro", DRB_I2C, usb, 2, 0x76, 100, e) && !strcmp(e.c_str(), "USB") && !g_pin.count(30));
  int boot[] = { 0, 30 };
  CHECK(!drvresAdd("altro", DRB_I2C, boot, 2, 0x76, 100, e));
  CHECK(!drvresAdd("altro", DRB_I2C, gp, 3, 0x76, 100, e));                              // I2C vuole 2 pin
  CHECK(!drvresAdd("altro", DRB_I2C, altri + 1, 1, 0x76, 100, e));
  int b2[] = { 30, 31 };
  CHECK(!drvresAdd("altro", DRB_I2C, b2, 2, 0x05, 100, e) && !drvresAdd("altro", DRB_I2C, b2, 2, 0x78, 100, e) && !drvresAdd("altro", DRB_I2C, b2, 2, 0x40, 250, e));   // indirizzo e velocita
  CHECK(!drvresAdd("rele1", DRB_I2C, b2, 2, 0x40, 100, e) && !strcmp(e.c_str(), "Nome gia usato"));   // e il nome di una periferica
  g_sensore = true;
  CHECK(!drvresAdd("altro", DRB_I2C, b2, 2, 0x40, 100, e) && e.length() && !g_pin.count(30));          // il bus I2C e del sensore a classi
  g_sensore = false;
  // SPI e gpio
  CHECK(drvresAdd("adc", DRB_SPI, spi, 4, 0, 1000, e) && drvresFind("adc", r) && r.npin == 4 && r.khz == 1000 && r.addr == 0);
  int s2[] = { 40, 41, 42, 43 };
  CHECK(!drvresAdd("adc2", DRB_SPI, s2, 4, 0, 50, e) && !drvresAdd("adc2", DRB_SPI, s2, 4, 0, 20000, e));
  CHECK(drvresAdd("led", DRB_GPIO, gp, 3, 0, 0, e) && drvresFind("led", r) && r.npin == 3 && r.khz == 0);
  int g9[] = { 1, 2, 3, 4, 8 };
  CHECK(!drvresAdd("x", DRB_GPIO, g9, 5, 0, 0, e) && !drvresAdd("x", DRB_GPIO, g9, 0, 0, 0, e) && !drvresAdd("MAIUSC", DRB_GPIO, g9, 1, 0, 0, e));
  int g1[] = { 14 };
  CHECK(drvresAdd("un", DRB_GPIO, g1, 1, 0, 0, e) && drvresCount() == 4);
  int g2[] = { 15 };
  CHECK(!drvresAdd("cinque", DRB_GPIO, g2, 1, 0, 0, e) && !g_pin.count(15));             // al massimo 4 driver
  CHECK(drvresGet(0, r) && !strcmp(r.name, "bmp") && !drvresGet(4, r) && !drvresGet(-1, r));

  // rilettura dal file (dopo un riavvio): i pin tornano presi
  std::string cfg = g_fs["/drvres.cfg"];
  g_pin.clear();
  g_pin[22] = "altro";                                                                   // un pin occupato da un'altra periferica: il driver bmp non si carica
  g_fs["/drvres.cfg"] = cfg + "rovinata 9 9\nzz 2 2 1 2 0 0 0 0\n";                      // righe rovinate o con bus sbagliato
  drvresInit();
  CHECK(!drvresFind("bmp", r) && drvresFind("adc", r) && drvresFind("led", r) && drvresFind("un", r) && !drvresFind("rovinata", r) && !drvresFind("zz", r));
  CHECK(g_pin[10] == "adc" && g_pin[22] == "altro" && !g_pin.count(21));                 // bmp non ha preso nessun pin
  g_pin.clear(); g_fs["/drvres.cfg"] = cfg;
  drvresInit();
  CHECK(drvresCount() == 4 && drvresFind("bmp", r) && r.pin[1] == 22 && g_pin[22] == "bmp");

  // rimozione
  g_scrivi = false;
  CHECK(!drvresRemove("bmp", e) && drvresFind("bmp", r) && g_pin.count(21));             // file non scritto: tutto com'era
  g_scrivi = true;
  CHECK(drvresRemove("bmp", e) && !drvresFind("bmp", r) && !g_pin.count(21) && !g_pin.count(22) && drvresCount() == 3);
  CHECK(!drvresRemove("bmp", e) && !drvresRemove(nullptr, e));
  CHECK(g_fs["/drvres.cfg"].find("bmp") == std::string::npos && g_fs["/drvres.cfg"].find("adc") != std::string::npos);
  CHECK(drvresRemove("adc", e) && drvresRemove("led", e) && drvresRemove("un", e) && !drvresCount() && !g_fs.count("/drvres.cfg"));   // lista vuota = file tolto
  return done("prova_drvres");
}
