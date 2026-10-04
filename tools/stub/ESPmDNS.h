#pragma once
#include <Arduino.h>
struct MDNSResponder{bool begin(const char*){return true;} void end(){} bool addService(const char*,const char*,uint16_t){return true;}}; extern MDNSResponder MDNS;
