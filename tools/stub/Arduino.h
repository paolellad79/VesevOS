#pragma once
// STUB per controllo sintassi su PC (non e il vero core Arduino-ESP32)
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string>
#include <ctype.h>
#include <functional>
#ifndef ARDUINO
#define ARDUINO 10800
#endif
#define ESP32 1
typedef bool boolean; typedef uint8_t byte;
#include <assert.h>
#include "pgmspace.h"
#include "WString.h"
#include "Print.h"
#include "Stream.h"
struct EspClass{uint32_t getFreeHeap(){return 0;} uint32_t getHeapSize(){return 0;} uint32_t getMinFreeHeap(){return 0;} uint32_t getMaxAllocHeap(){return 0;} uint32_t getFreePsram(){return 0;} uint32_t getPsramSize(){return 0;} const char*getChipModel(){return "";} uint8_t getChipRevision(){return 0;} uint8_t getChipCores(){return 2;} const char*getSdkVersion(){return "";} const char*getCoreVersion(){return "";} void restart(){} uint32_t getFlashChipSize(){return 0;} uint32_t getCpuFreqMHz(){return 0;} uint64_t getEfuseMac(){return 0;} uint32_t getSketchSize(){return 0;} uint32_t getFreeSketchSpace(){return 0;}};
extern EspClass ESP;
unsigned long millis(); unsigned long micros(); void delay(unsigned long); uint32_t getCpuFrequencyMhz(); bool setCpuFrequencyMhz(uint32_t);
void pinMode(uint8_t,uint8_t); void digitalWrite(uint8_t,uint8_t); int digitalRead(uint8_t); void yield();
void enableLoopWDT(); void disableLoopWDT(); void feedLoopWDT();
#define HIGH 1
#define LOW 0
#define INPUT 1
#define OUTPUT 3
#define INPUT_PULLUP 5
#define INPUT_PULLDOWN 9
struct HWCDC: public Stream{int peek(){return 0;} int available(){return 0;} int read(){return 0;} void flush(){} size_t write(uint8_t){return 1;} size_t write(const uint8_t*b,size_t n){return n;} void begin(unsigned long=0){} void updateBaudRate(unsigned long){} void setTxTimeoutMs(uint32_t){} operator bool(){return true;} int availableForWrite(){return 64;}}; extern HWCDC Serial;
#define constrain(a,b,c) ((a)<(b)?(b):(a)>(c)?(c):(a))
#include <algorithm>
using std::min; using std::max;
#include "freertos/FreeRTOS.h"
void rgbLedWrite(uint8_t,uint8_t,uint8_t,uint8_t); float temperatureRead(); inline bool isHexadecimalDigit(char){return true;} inline bool isAlphaNumeric(char){return true;} inline bool isAlpha(char){return true;} inline bool isPrintable(char){return true;}
TaskHandle_t xTaskGetHandle(const char*); void vTaskDelete(TaskHandle_t); UBaseType_t uxTaskGetStackHighWaterMark(TaskHandle_t);
inline bool isDigit(char c){return c>=48&&c<=57;}
#include <math.h>
inline bool psramFound(){return true;} inline void* ps_malloc(size_t n){return malloc(n);}
#include "esp_random.h"
#include "IPAddress.h"
