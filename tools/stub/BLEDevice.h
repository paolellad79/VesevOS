#pragma once
// STUB ricavato dalla libreria BLE del core Arduino-ESP32 3.3 (NimBLE), solo cio che usa VesevOS
#include <Arduino.h>
#include <map>
#define ESP_IO_CAP_OUT 0
#define ESP_IO_CAP_NONE 3
class BLEServer; class BLEService; class BLECharacteristic; class BLEAdvertising;
class BLECharacteristicCallbacks{public: virtual ~BLECharacteristicCallbacks(){} virtual void onRead(BLECharacteristic*){} virtual void onWrite(BLECharacteristic*){}};
class BLEServerCallbacks{public: virtual ~BLEServerCallbacks(){} virtual void onConnect(BLEServer*){} virtual void onDisconnect(BLEServer*){}};
class BLECharacteristic{public: static const uint32_t PROPERTY_READ=1, PROPERTY_READ_ENC=0x200, PROPERTY_READ_AUTHEN=0x400, PROPERTY_WRITE=8, PROPERTY_WRITE_NR=4, PROPERTY_WRITE_ENC=0x1000, PROPERTY_WRITE_AUTHEN=0x2000, PROPERTY_NOTIFY=0x10;
 String getValue()const{return "";} void setValue(const String&){} void setValue(const uint8_t*,size_t){} void notify(bool=true){} void setCallbacks(BLECharacteristicCallbacks*){}};
class BLEService{public: BLECharacteristic* createCharacteristic(const char*,uint32_t){return nullptr;} void start(){}};
class BLEServer{public: BLEService* createService(const char*){return nullptr;} void setCallbacks(BLEServerCallbacks*){} void startAdvertising(){}};
class BLEAdvertising{public: void addServiceUUID(const char*){} bool stop(){return true;} void start(){}};
class BLEDevice{public: static bool init(String=""){return true;} static void deinit(bool=false){} static BLEServer* createServer(){return nullptr;} static BLEAdvertising* getAdvertising(){return nullptr;} static void startAdvertising(){} static void stopAdvertising(){} static bool getInitialized(){return true;}};
