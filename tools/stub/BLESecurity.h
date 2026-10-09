#pragma once
#include "BLEDevice.h"
class BLESecurity{public: static uint32_t setPassKey(bool=false,uint32_t=123456){return 0;} static void setCapability(uint8_t){} static void setAuthenticationMode(bool,bool,bool){} static void setAuthenticationMode(uint8_t){}};
