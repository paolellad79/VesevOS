// VesevOS - prove sul computer: il file di scheda (boards/) da' gli stessi numeri di prima della 1.7.62
// (pin esistenti, riservati, ADC, I2C, pin consigliati). I valori "di prima" sono scritti qui a mano di proposito.
#include <Arduino.h>
#include "vos_board.h"
#include "vos_pins.h"
#include "vos_io.h"
#include "vos_config.h"
#include "vos_common.h"
#include "vos_dev.h"
#include "vos_drv_gpio.h"
#include <FS.h>
#include "mini.h"
#include <string.h>

// ---- finti pezzi (il resto e vero: vos_pins.cpp e vos_io.cpp) ----
EspClass ESP;
// file system finto in memoria (iocfg/FS.h): run.sh compila questo test con quello
FakeFsFaults g_ff;
namespace fs { std::map<std::string, std::shared_ptr<FakeNode>> g_fakeFiles; }
void drvGpioMode(int, DrvGpioMode) {}
void drvGpioWrite(int, bool) {}
bool drvGpioRead(int) { return false; }
int  drvGpioAnalog(int) { return 0; }
const DevOps* devFind(const char*) { return nullptr; }

static const int8_t OLD_C[]  = {1, 2, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 21, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 47};
static const int8_t OLD_CI[] = {8, 9, 4, 5, 6, 7, 1, 2, 10, 11, 12, 13, 14, 15, 16, 17, 18, 21, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 47};

// la regola di prima, copiata a mano
static const char* oldBlock(int g) {
  bool ex = (g >= 0 && g <= 21) || (g >= 33 && g <= 48);
  if (!ex) return "Pin inesistente su questa scheda";
  if (g == 19 || g == 20) return "USB: serve al collegamento con il PC";
  if (g == 48) return "LED RGB della scheda";
  if (g == 0) return "Tasto BOOT";
  if (g == 3 || g == 45 || g == 46) return "Pin di avvio: meglio non toccarlo";
  return nullptr;
}

int main() {
  // costanti
  CHECK(VOS_PIN_LED_RGB == 48 && VOS_PIN_BOOT == 0 && VOS_PIN_USB_DM == 19 && VOS_PIN_USB_DP == 20);
  CHECK(VOS_GPIO_MAX == 48 && VOS_GPIO_COUNT == 49 && VOS_ADC_FIRST == 1 && VOS_ADC_LAST == 10 && VOS_I2C_SDA == 8 && VOS_I2C_SCL == 9);
  CHECK(VOS_FLASH_MB == 4 && VOS_PSRAM_MB == 2 && VOS_HAS_PSRAM && VOS_HAS_USB_NATIVE && VOS_HAS_BLE && VOS_HAS_WIFI);
  CHECK(strcmp(VOS_BOARD, "ESP32-S3 SuperMini") == 0 && strcmp(VOS_LED_TYPE, "WS2812") == 0);
  CHECK(sizeof(cfg.pinNote) / sizeof(cfg.pinNote[0]) == 49);
  // GPIO esistenti e riservati: uguali a prima per -2..50 (pinTestBlock e quello vero di vos_pins.cpp)
  int existing = 0;
  for (int g = -2; g <= 50; g++) {
    const char* a = pinTestBlock(g); const char* b = oldBlock(g);
    CHECK((a == nullptr) == (b == nullptr));
    if (a && b) CHECK(strcmp(a, b) == 0);
    if (g >= 0 && g <= 48 && strcmp(b ? b : "", "Pin inesistente su questa scheda") != 0) existing++;
  }
  CHECK(existing == 22 + 16);                                       // 0-21 e 33-48
  // mappa dei pin: stessi GPIO di prima, nell'ordine
  String m = pinMapJson(); int n = 0;
  for (int g = 0; g <= 48; g++) { bool ex = (g >= 0 && g <= 21) || (g >= 33 && g <= 48); String k = "{\"g\":" + String(g) + ","; CHECK((m.indexOf(k) >= 0) == ex); n += ex; }
  CHECK(n == 38);
  // pin consigliati: stesso elenco e stesso ordine di prima, per ogni classe
  int out[64];
  for (int cls = IC_RELE; cls <= IC_I2C; cls++) {
    int k = ioSuggest((uint8_t)cls, 64, out);
    int e[64], ek = 0;
    const int8_t* L = cls == IC_I2C ? OLD_CI : OLD_C;
    for (size_t i = 0; i < sizeof(OLD_C); i++) {
      int g = L[i];
      if (oldBlock(g)) continue;
      if (cls == IC_ADC && !(g >= 1 && g <= 10)) continue;
      e[ek++] = g;
    }
    CHECK(k == ek);
    for (int i = 0; i < k && i < ek; i++) CHECK(out[i] == e[i]);
  }
  // I2C: prima 8 e 9; analogico: solo 1-10
  CHECK(ioSuggest(IC_I2C, 2, out) == 2 && out[0] == 8 && out[1] == 9);
  { int k = ioSuggest(IC_ADC, 64, out); bool ok = k > 0; for (int i = 0; i < k; i++) ok = ok && out[i] >= 1 && out[i] <= 10; CHECK(ok); }
  return done("board");
}
