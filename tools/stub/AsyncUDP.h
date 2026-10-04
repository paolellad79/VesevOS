#pragma once
#include <WiFi.h>
#include <functional>
class AsyncUDPPacket{public: uint8_t* data(){return 0;} size_t length(){return 0;} IPAddress remoteIP(){return IPAddress();} uint16_t remotePort(){return 0;} size_t write(const uint8_t*,size_t){return 0;}};
class AsyncUDP{public: bool listen(uint16_t){return true;} void onPacket(std::function<void(AsyncUDPPacket&)>){} void close(){} size_t writeTo(const uint8_t*,size_t,const IPAddress&,uint16_t){return 0;}};
