#pragma once
// Finto SD.h per compila.sh (solo la forma delle funzioni della libreria SD di Arduino-ESP32)
#include <Arduino.h>
#include <FS.h>
#include <SPI.h>
typedef enum { CARD_NONE, CARD_MMC, CARD_SD, CARD_SDHC, CARD_UNKNOWN } sdcard_type_t;
class SDFS : public fs::FS {
 public:
  bool begin(uint8_t ss = 5, SPIClass& spi = *(SPIClass*)nullptr, uint32_t hz = 4000000, const char* mp = "/sd", uint8_t maxFiles = 5, bool format = false) { return false; }
  void end() {}
  sdcard_type_t cardType() { return CARD_NONE; }
  uint64_t cardSize() { return 0; }
  uint64_t totalBytes() { return 0; }
  uint64_t usedBytes() { return 0; }
};
inline SDFS SD;
