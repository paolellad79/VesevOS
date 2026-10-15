#pragma once
// STUB per controllo sintassi su PC (non e il vero Wire di Arduino-ESP32)
#include <Arduino.h>
struct TwoWire {
  bool begin(int, int, uint32_t = 0) { return true; } bool end() { return true; } void setTimeOut(uint16_t) {}
  void beginTransmission(uint8_t) {} size_t write(uint8_t) { return 1; } uint8_t endTransmission(bool = true) { return 0; }
  size_t requestFrom(uint8_t, size_t n, bool = true) { return n; } int available() { return 0; } int read() { return 0; }
};
extern TwoWire Wire;
