#pragma once
#include <Arduino.h>
#define UPDATE_SIZE_UNKNOWN 0xFFFFFFFF
#define U_FLASH 0
class UpdateClass { public:
  bool begin(size_t size = UPDATE_SIZE_UNKNOWN, int cmd = U_FLASH);
  size_t write(uint8_t* d, size_t l);
  bool end(bool evenIfRemaining = false);
  void abort();
  const char* errorString();
};
extern UpdateClass Update;
