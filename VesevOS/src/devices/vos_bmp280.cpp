// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_bmp280.cpp (vedi vos_bmp280.h)
#include "vos_bmp280.h"
#include "../drivers/vos_drv_i2c.h"

#define R_ID    0xD0
#define R_CAL   0x88
#define R_CTRL  0xF4
#define R_CONF  0xF5
#define R_DATA  0xF7

static uint16_t u16(const uint8_t* b) { return (uint16_t)(b[0] | (b[1] << 8)); }
static int16_t  s16(const uint8_t* b) { return (int16_t)u16(b); }

void bmp280ParseCal(const uint8_t* b, Bmp280Cal& c) {
  c.T1 = u16(b); c.T2 = s16(b + 2); c.T3 = s16(b + 4);
  c.P1 = u16(b + 6); c.P2 = s16(b + 8); c.P3 = s16(b + 10); c.P4 = s16(b + 12); c.P5 = s16(b + 14);
  c.P6 = s16(b + 16); c.P7 = s16(b + 18); c.P8 = s16(b + 20); c.P9 = s16(b + 22);
}

void bmp280Compensate(const Bmp280Cal& c, int32_t adcT, int32_t adcP, int32_t& t100, uint32_t& pPa) {
  int32_t v1 = (((adcT >> 3) - ((int32_t)c.T1 << 1)) * (int32_t)c.T2) >> 11;
  int32_t d = (adcT >> 4) - (int32_t)c.T1;
  int32_t v2 = (((d * d) >> 12) * (int32_t)c.T3) >> 14;
  int32_t tFine = v1 + v2;
  t100 = (tFine * 5 + 128) >> 8;
  int64_t p1 = (int64_t)tFine - 128000;
  int64_t p2 = p1 * p1 * (int64_t)c.P6;
  p2 += (p1 * (int64_t)c.P5) * 131072;               // 2^17 (con * e non con <<: il valore puo essere negativo)
  p2 += ((int64_t)c.P4) * 34359738368LL;                // 2^35
  p1 = ((p1 * p1 * (int64_t)c.P3) >> 8) + ((p1 * (int64_t)c.P2) * 4096);
  p1 = ((((int64_t)1 << 47) + p1) * (int64_t)c.P1) >> 33;
  if (p1 == 0) { pPa = 0; return; }
  int64_t p = 1048576 - adcP;
  p = (((p * 2147483648LL) - p2) * 3125) / p1;       // 2^31
  p1 = ((int64_t)c.P9 * (p >> 13) * (p >> 13)) >> 25;
  p2 = ((int64_t)c.P8 * p) >> 19;
  p = ((p + p1 + p2) >> 8) + ((int64_t)c.P7 * 16);
  pPa = (uint32_t)((p + 128) >> 8);                       // da Q24.8 a pascal, arrotondato
}

int bmp280Begin(uint8_t addr, Bmp280Cal& c, uint8_t& chipId) {
  uint8_t id = 0;
  uint8_t e = drvI2cReadReg(addr, R_ID, &id, 1);
  if (e == DI_NOBUS) return BMP_BUS;
  if (e) return BMP_NO_ANSWER;
  chipId = id;
  if (id != 0x58 && id != 0x56 && id != 0x57 && id != 0x60) return BMP_WRONG_CHIP;    // BMP280 (0x56/0x57 prototipi, 0x58) e BME280 (0x60)
  uint8_t raw[24];
  if (drvI2cReadReg(addr, R_CAL, raw, sizeof raw)) return BMP_NO_ANSWER;
  bmp280ParseCal(raw, c);
  if (c.T1 == 0 || c.P1 == 0) return BMP_WRONG_CHIP;                                 // taratura vuota: non e un sensore vero
  uint8_t conf = 0xA0;                                                               // pausa 1 s, filtro spento
  uint8_t ctrl = 0x2F;                                                               // temperatura x1, pressione x4, misura continua
  if (drvI2cWriteReg(addr, R_CONF, &conf, 1)) return BMP_NO_ANSWER;
  if (drvI2cWriteReg(addr, R_CTRL, &ctrl, 1)) return BMP_NO_ANSWER;
  return BMP_OK;
}

int bmp280Sample(uint8_t addr, const Bmp280Cal& c, int32_t& t100, uint32_t& pPa) {
  uint8_t b[6];
  uint8_t e = drvI2cReadReg(addr, R_DATA, b, sizeof b);
  if (e == DI_NOBUS) return BMP_BUS;
  if (e) return BMP_NO_ANSWER;
  int32_t adcP = ((int32_t)b[0] << 12) | ((int32_t)b[1] << 4) | (b[2] >> 4);
  int32_t adcT = ((int32_t)b[3] << 12) | ((int32_t)b[4] << 4) | (b[5] >> 4);
  if (adcT == 0x80000 || adcP == 0x80000) return BMP_NOT_READY;                      // valore "misura saltata"
  bmp280Compensate(c, adcT, adcP, t100, pPa);
  return BMP_OK;
}
