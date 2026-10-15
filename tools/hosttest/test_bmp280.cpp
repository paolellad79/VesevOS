// VesevOS - prove sul computer: sensore BMP280 (vos_bmp280.cpp) con bus I2C finto
#include <Arduino.h>
#include "vos_bmp280.h"
#include "vos_drv_i2c.h"
#include "fake_i2c.h"
#include "mini.h"

int main() {
  // vettore del foglio dati Bosch: T = 25.08 C, P = 100653 Pa
  Bmp280Cal c = {27504, 26435, -1000, 36477, -10685, 3024, 2855, 140, -7, 15500, -14600, 6000};
  int32_t t = 0; uint32_t p = 0;
  bmp280Compensate(c, 519888, 415148, t, p);
  CHECK(t == 2508);
  CHECK(p >= 100650 && p <= 100656);
  // lettura dai registri finti
  fakeI2cReset(); fakeBmpLoad(0x76); g_fk.bus = true;
  Bmp280Cal k; uint8_t id = 0;
  CHECK(bmp280Begin(0x76, k, id) == BMP_OK && id == 0x58 && k.T1 == 27504 && k.T3 == -1000 && k.P2 == -10685 && k.P9 == 6000);
  CHECK(g_fk.lastW[0xF5] == 0xA0 && g_fk.lastW[0xF4] == 0x2F);                       // filtro spento, pausa 1 s, misura continua
  t = 0; p = 0;
  CHECK(bmp280Sample(0x76, k, t, p) == BMP_OK && t == 2508 && p >= 100650 && p <= 100656);
  // sensore assente all'altro indirizzo: nessuna risposta, nessun blocco
  CHECK(bmp280Begin(0x77, k, id) == BMP_NO_ANSWER);
  CHECK(bmp280Sample(0x77, k, t, p) == BMP_NO_ANSWER);
  // misura non ancora pronta (0x80000)
  g_fk.reg[0xF7] = 0x80; g_fk.reg[0xF8] = 0; g_fk.reg[0xF9] = 0;
  CHECK(bmp280Sample(0x76, k, t, p) == BMP_NOT_READY);
  // BME280 (0x60) accettato; chip sconosciuto no; taratura vuota no
  fakeBmpLoad(0x76); g_fk.bus = true; g_fk.reg[0xD0] = 0x60;
  CHECK(bmp280Begin(0x76, k, id) == BMP_OK && id == 0x60);
  g_fk.reg[0xD0] = 0x55; CHECK(bmp280Begin(0x76, k, id) == BMP_WRONG_CHIP && id == 0x55);
  fakeBmpLoad(0x76); g_fk.bus = true; for (int i = 0x88; i < 0xA0; i++) g_fk.reg[i] = 0;
  CHECK(bmp280Begin(0x76, k, id) == BMP_WRONG_CHIP);
  // timeout in lettura e bus spento
  fakeBmpLoad(0x76); g_fk.bus = true; g_fk.failReads = true;
  CHECK(bmp280Begin(0x76, k, id) == BMP_NO_ANSWER && bmp280Sample(0x76, k, t, p) == BMP_NO_ANSWER);
  g_fk.bus = false; CHECK(bmp280Begin(0x76, k, id) == BMP_BUS && bmp280Sample(0x76, k, t, p) == BMP_BUS);
  // temperature negative e pressione bassa non devono rompere il calcolo
  bmp280Compensate(c, 400000, 300000, t, p); CHECK(t < 2508);
  return done("bmp280");
}
