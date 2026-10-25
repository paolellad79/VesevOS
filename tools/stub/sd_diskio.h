// Stub di sd_diskio.h (libreria SD di Arduino-ESP32) per compila.sh: solo le dichiarazioni usate da vos_drv_sd.cpp
#pragma once
#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
inline uint8_t sdcard_init(uint8_t, SPIClass*, int) { return 0xFF; }
inline uint8_t sdcard_uninit(uint8_t) { return 0; }
inline sdcard_type_t sdcard_type(uint8_t) { return CARD_NONE; }
inline bool sd_read_raw(uint8_t, uint8_t*, uint32_t) { return false; }
