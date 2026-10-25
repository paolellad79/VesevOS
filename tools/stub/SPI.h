#pragma once
// Finto SPI.h per compila.sh (solo la forma delle funzioni)
#include <Arduino.h>
#ifndef FSPI
#define FSPI 1
#endif
class SPIClass { public: SPIClass(uint8_t bus = FSPI) {} void begin(int sck = -1, int miso = -1, int mosi = -1, int ss = -1) {} void end() {} };
