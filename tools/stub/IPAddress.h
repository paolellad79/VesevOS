#pragma once
#include <stdint.h>
class String;
struct IPAddress{uint8_t b[4]={0,0,0,0};IPAddress(){} IPAddress(uint8_t a,uint8_t c,uint8_t d,uint8_t e){b[0]=a;b[1]=c;b[2]=d;b[3]=e;} IPAddress(uint32_t){} operator uint32_t()const{return 0;} String toString()const; bool fromString(const char*){return true;} bool fromString(const String&){return true;} uint8_t operator[](int i)const{return b[i];}};
