#pragma once
// Bus I2C finto per le prove sul computer: registri in memoria, errori a comando, contatori delle chiamate.
#include <stdint.h>
struct FakeI2c {
  bool bus; int sda, scl; uint32_t hz;
  bool beginFail;            // drvI2cBegin fallisce
  bool present[128];         // indirizzi che rispondono
  bool failReads;            // le letture danno NACK
  uint8_t reg[256];          // registri del sensore (stessi per ogni indirizzo presente)
  int begins, ends, reads, writes;
  uint8_t lastW[256];
};
extern FakeI2c g_fk;
void fakeI2cReset();
void fakeBmpLoad(int addr);  // sensore BMP280 con la taratura e le misure dell'esempio del foglio dati Bosch (T=25.08 C, P=100653 Pa)
