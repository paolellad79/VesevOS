// Prova sul computer del nucleo hardware dei driver ELF (packages/vos_hw.cpp, VERO) con finti driver e finto registro dei pin dati ai driver.
// Prova: pin e bus solo del driver nominato, indici, ordine "modo prima di scrivere", bus I2C unico (occupato dal sensore a classi, ripreso da un altro
// driver), tetti dei blocchi, errori dei driver, rilascio. NON prova l'hardware vero (serve la scheda).
#include <stdio.h>
#include <string.h>
#include "vos_hw.h"
#include "vos_drvres.h"
#include "vos_drv_gpio.h"
#include "vos_drv_i2c.h"
#include "mini.h"

// ---- finti ----
extern "C++" int64_t esp_timer_get_time() { return 5000000; }
extern "C++" void esp_rom_delay_us(uint32_t) {}
static DrvRes g_res[3];
static bool g_sensoreIo = false, g_i2cOk = true, g_spiOk = true, g_i2cBegin = false;
static int g_nI2cBegin = 0, g_nSpiBegin = 0, g_nI2cEnd = 0, g_nSpiEnd = 0, g_lastSda = -1, g_pinMode[50], g_pinLevel[50];
static unsigned char g_spiTx[64];
bool drvresFind(const char* n, DrvRes& o) { for (auto& r : g_res) if (r.name[0] && !strcmp(r.name, n)) { o = r; return true; } return false; }
bool ioHasI2c() { return g_sensoreIo; }
void drvGpioMode(int g, DrvGpioMode m) { g_pinMode[g] = (int)m; }
void drvGpioWrite(int g, bool h) { g_pinLevel[g] = h; }
bool drvGpioRead(int g) { return g_pinLevel[g] != 0; }
bool drvI2cBegin(int sda, int, uint32_t hz) { if (!g_i2cOk) return false; g_nI2cBegin++; g_i2cBegin = true; g_lastSda = sda; (void)hz; return true; }
void drvI2cEnd() { g_nI2cEnd++; g_i2cBegin = false; }
uint8_t drvI2cReadReg(uint8_t addr, uint8_t reg, uint8_t* b, size_t n) { if (addr == 0x7f) return DI_NACK; for (size_t i = 0; i < n; i++) b[i] = (uint8_t)(reg + i); return DI_OK; }
uint8_t drvI2cWriteReg(uint8_t addr, uint8_t, const uint8_t*, size_t) { return addr == 0x7f ? DI_NACK : DI_OK; }
bool drvI2cProbe(uint8_t addr) { return addr == 0x76; }
bool drvSpiBegin(int, int, int, int, uint32_t) { if (!g_spiOk) return false; g_nSpiBegin++; return true; }
void drvSpiEnd() { g_nSpiEnd++; }
bool drvSpiTransfer(const uint8_t* tx, uint8_t* rx, size_t n) { for (size_t i = 0; i < n; i++) { g_spiTx[i] = tx ? tx[i] : 0; if (rx) rx[i] = (uint8_t)~g_spiTx[i]; } return true; }

static DrvRes mk(const char* nome, uint8_t bus, int npin, int p0, int addr, int khz) {
  DrvRes r; memset(&r, 0, sizeof r);
  snprintf(r.name, sizeof r.name, "%s", nome); r.bus = bus; r.npin = (uint8_t)npin;
  for (int i = 0; i < npin; i++) r.pin[i] = (int8_t)(p0 + i);
  r.addr = (uint8_t)addr; r.khz = (uint16_t)khz;
  return r;
}

int main() {
  g_res[0] = mk("led", DRB_GPIO, 2, 5, 0, 0);
  g_res[1] = mk("bmp", DRB_I2C, 2, 21, 0x76, 100);
  g_res[2] = mk("adc", DRB_SPI, 4, 10, 0, 1000);
  uint8_t b[40];

  // gpio
  CHECK(hwGpioMode("bmp", 0, HW_MODO_OUT) == HW_E_NONTROVATO && hwGpioMode("zzz", 0, HW_MODO_IN) == HW_E_NONTROVATO && hwGpioMode(nullptr, 0, 0) == HW_E_NONTROVATO);
  CHECK(hwGpioMode("led", 2, HW_MODO_IN) == HW_E_ARG && hwGpioMode("led", -1, HW_MODO_IN) == HW_E_ARG && hwGpioMode("led", 0, 9) == HW_E_ARG);
  CHECK(hwGpioWrite("led", 0, 1) == HW_E_ARG);                                     // prima il modo
  CHECK(hwGpioMode("led", 0, HW_MODO_OUT) == 0 && g_pinMode[5] == 3 && hwGpioWrite("led", 0, 7) == 0 && g_pinLevel[5] == 1);
  CHECK(hwGpioWrite("led", 1, 1) == HW_E_ARG);                                     // il secondo pin non e in uscita
  CHECK(hwGpioMode("led", 1, HW_MODO_PULLUP) == 0 && g_pinMode[6] == 1 && hwGpioRead("led", 1) == 0 && hwGpioWrite("led", 0, 0) == 0 && hwGpioRead("led", 0) == 0);
  CHECK(hwGpioRead("led", 5) == HW_E_ARG && hwGpioRead("adc", 0) == HW_E_NONTROVATO);
  hwRilascia("led");
  CHECK(g_pinMode[5] == 0 && hwGpioWrite("led", 0, 1) == HW_E_ARG);                // le uscite tornano in ingresso e il modo si dimentica

  // i2c
  CHECK(hwI2cProbe("led") == HW_E_NONTROVATO && hwI2cRead("led", 0, b, 1) == HW_E_NONTROVATO);
  CHECK(hwI2cProbe("bmp") == 1 && g_nI2cBegin == 1 && g_lastSda == 21);
  CHECK(hwI2cRead("bmp", 0xD0, b, 3) == 3 && b[0] == 0xD0 && b[2] == 0xD2 && g_nI2cBegin == 1);   // il bus resta acceso
  CHECK(hwI2cWrite("bmp", 0xF4, b, 1) == 1 && hwI2cWrite("bmp", 0xF4, nullptr, 0) == 0);
  CHECK(hwI2cRead("bmp", 0, b, 0) == HW_E_ARG && hwI2cRead("bmp", 0, b, 33) == HW_E_GRANDE && hwI2cRead("bmp", 256, b, 1) == HW_E_ARG && hwI2cRead("bmp", -1, b, 1) == HW_E_ARG);
  CHECK(hwI2cRead("bmp", 0, nullptr, 1) == HW_E_ARG && hwI2cWrite("bmp", 0, nullptr, 2) == HW_E_ARG && hwI2cWrite("bmp", 0, b, 31) == HW_E_GRANDE);
  g_res[1].addr = 0x7f;
  CHECK(hwI2cRead("bmp", 0, b, 1) == HW_E_IO && hwI2cWrite("bmp", 0, b, 1) == HW_E_IO);   // il sensore non risponde
  g_res[1].addr = 0x76;
  g_sensoreIo = true; g_nI2cBegin = 0; hwRilascia("bmp");                          // il bus e del sensore delle periferiche: non si prende
  CHECK(g_nI2cEnd == 1 && hwI2cProbe("bmp") == HW_E_OCCUPATO && g_nI2cBegin == 0);
  g_sensoreIo = false; g_i2cOk = false;
  CHECK(hwI2cProbe("bmp") == HW_E_OCCUPATO);                                       // il bus non parte
  g_i2cOk = true;
  CHECK(hwI2cProbe("bmp") == 1 && g_nI2cBegin == 1);
  g_res[0] = mk("led", DRB_I2C, 2, 30, 0x40, 400);                                 // un secondo driver I2C: prende il bus
  CHECK(hwI2cProbe("led") == 0 && g_nI2cBegin == 2 && g_lastSda == 30);
  CHECK(hwI2cProbe("bmp") == 1 && g_nI2cBegin == 3 && g_lastSda == 21);            // e poi lo riprende l'altro

  // spi
  CHECK(hwSpiTransfer("bmp", b, b, 2) == HW_E_NONTROVATO && hwSpiTransfer("adc", b, b, 0) == HW_E_ARG && hwSpiTransfer("adc", b, b, 65) == HW_E_GRANDE);
  unsigned char tx[3] = { 1, 2, 3 }, rx[3] = { 0, 0, 0 };
  CHECK(hwSpiTransfer("adc", tx, rx, 3) == 3 && rx[0] == (unsigned char)~1 && rx[2] == (unsigned char)~3 && g_nSpiBegin == 1);
  CHECK(hwSpiTransfer("adc", nullptr, rx, 2) == 2 && g_spiTx[0] == 0 && hwSpiTransfer("adc", tx, nullptr, 2) == 2 && g_nSpiBegin == 1);   // zeri e risposta buttata
  hwRilascia("adc");
  CHECK(g_nSpiEnd == 1);
  g_spiOk = false; CHECK(hwSpiTransfer("adc", tx, rx, 1) == HW_E_OCCUPATO);

  // tempo
  CHECK(hwDelayUs(0) == 0 && hwDelayUs(-5) == 0 && hwDelayUs(100) == 100 && hwDelayUs(99999) == HW_DELAY_US_MAX);
  hwUptime();
  return done("prova_hw");
}
