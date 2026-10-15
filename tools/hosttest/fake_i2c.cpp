// Bus I2C finto (vedi fake_i2c.h): sostituisce vos_drv_i2c.cpp nelle prove sul computer
#include <Arduino.h>
#include "vos_drv_i2c.h"
#include "fake_i2c.h"
#include <string.h>

FakeI2c g_fk;
void fakeI2cReset() { memset(&g_fk, 0, sizeof g_fk); }
static void p16(int a, int v) { g_fk.reg[a] = (uint8_t)(v & 0xFF); g_fk.reg[a + 1] = (uint8_t)((v >> 8) & 0xFF); }
void fakeBmpLoad(int addr) {
  memset(g_fk.reg, 0, sizeof g_fk.reg);
  g_fk.present[addr] = true; g_fk.reg[0xD0] = 0x58;
  const int cal[12] = {27504, 26435, -1000, 36477, -10685, 3024, 2855, 140, -7, 15500, -14600, 6000};
  for (int i = 0; i < 12; i++) p16(0x88 + 2 * i, cal[i]);
  int adcP = 415148, adcT = 519888;
  g_fk.reg[0xF7] = (adcP >> 12) & 0xFF; g_fk.reg[0xF8] = (adcP >> 4) & 0xFF; g_fk.reg[0xF9] = (adcP & 0xF) << 4;
  g_fk.reg[0xFA] = (adcT >> 12) & 0xFF; g_fk.reg[0xFB] = (adcT >> 4) & 0xFF; g_fk.reg[0xFC] = (adcT & 0xF) << 4;
}

bool drvI2cBegin(int sda, int scl, uint32_t hz) {
  g_fk.begins++;
  if (g_fk.beginFail) return false;
  g_fk.bus = true; g_fk.sda = sda; g_fk.scl = scl; g_fk.hz = hz; return true;
}
void drvI2cEnd() { if (g_fk.bus) g_fk.ends++; g_fk.bus = false; }
uint8_t drvI2cReadReg(uint8_t addr, uint8_t reg, uint8_t* buf, size_t n) {
  if (!g_fk.bus) return DI_NOBUS;
  if (!n || n > 32) return DI_OTHER;
  g_fk.reads++;
  if (addr > 127 || !g_fk.present[addr]) return DI_NACK;
  if (g_fk.failReads) return DI_TIMEOUT;
  for (size_t i = 0; i < n; i++) buf[i] = g_fk.reg[(reg + i) & 0xFF];
  return DI_OK;
}
uint8_t drvI2cWriteReg(uint8_t addr, uint8_t reg, const uint8_t* buf, size_t n) {
  if (!g_fk.bus) return DI_NOBUS;
  g_fk.writes++;
  if (addr > 127 || !g_fk.present[addr]) return DI_NACK;
  for (size_t i = 0; i < n; i++) { g_fk.lastW[(reg + i) & 0xFF] = buf[i]; }
  return DI_OK;
}
