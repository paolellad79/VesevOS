#pragma once
#include <Arduino.h>
namespace fs {
class File: public Stream{public: int peek(){return 0;} size_t write(uint8_t){return 1;} size_t write(const uint8_t*b,size_t n){return n;} int read(){return 0;} size_t read(uint8_t*b,size_t n){return n;} int available(){return 0;} size_t size()const{return 0;} void close(){} operator bool()const{return true;} bool isDirectory(){return false;} File openNextFile(const char* m="r"){return File();} const char*name()const{return "";} const char*path()const{return "";} bool seek(uint32_t){return true;} size_t position()const{return 0;} String readString(){return "";} String readStringUntil(char){return "";} time_t getLastWrite(){return 0;} void flush(){} };
class FS{public: File open(const String&,const char* ="r",bool=false){return File();} File open(const char*,const char* ="r",bool=false){return File();} bool exists(const String&){return true;} bool exists(const char*){return true;} bool remove(const String&){return true;} bool remove(const char*){return true;} bool rename(const String&,const String&){return true;} bool mkdir(const String&){return true;} bool rmdir(const String&){return true;} size_t totalBytes(){return 0;} size_t usedBytes(){return 0;} bool begin(bool=false,const char* ="/littlefs",uint8_t=10,const char* ="spiffs"){return true;} bool format(){return true;} void end(){} };
}
using fs::File; using fs::FS;
#include <time.h>
