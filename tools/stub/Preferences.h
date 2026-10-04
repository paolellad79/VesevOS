#pragma once
#include <Arduino.h>
class Preferences{public: bool begin(const char*,bool=false,const char* =nullptr){return true;} void end(){} bool clear(){return true;} bool isKey(const char*){return false;}
 uint32_t getUInt(const char*,uint32_t d=0){return d;} size_t putUInt(const char*,uint32_t){return 4;} int32_t getInt(const char*,int32_t d=0){return d;} size_t putInt(const char*,int32_t){return 4;}
 String getString(const char*,String d=String()){return d;} size_t getString(const char*,char*,size_t){return 0;} size_t putString(const char*,const String&){return 0;} size_t putString(const char*,const char*){return 0;}
 uint8_t getUChar(const char*,uint8_t d=0){return d;} size_t putUChar(const char*,uint8_t){return 1;} bool remove(const char*){return true;} bool getBool(const char*,bool d=false){return d;} size_t putBool(const char*,bool){return 1;}
 size_t getBytesLength(const char*){return 0;} size_t getBytes(const char*,void*,size_t){return 0;} size_t putBytes(const char*,const void*,size_t){return 0;} uint64_t getULong64(const char*,uint64_t d=0){return d;} size_t putULong64(const char*,uint64_t){return 8;}};
