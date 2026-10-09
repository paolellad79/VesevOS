// stub minimo NimBLE-Arduino 2.x (solo per controllo sintassi)
#pragma once
#include <Arduino.h>
#include <string>
#define BLE_HS_IO_DISPLAY_ONLY 0
namespace NIMBLE_PROPERTY { enum : uint32_t { READ=1, WRITE=2, NOTIFY=4, READ_ENC=8, READ_AUTHEN=16, WRITE_ENC=32, WRITE_AUTHEN=64 }; }
struct NimBLEConnInfo {};
struct NimBLEAttValue { const uint8_t* data() const { return nullptr; } size_t length() const { return 0; } };
class NimBLECharacteristic;
class NimBLECharacteristicCallbacks { public: virtual ~NimBLECharacteristicCallbacks() {} virtual void onWrite(NimBLECharacteristic*, NimBLEConnInfo&) {} };
class NimBLECharacteristic { public: NimBLEAttValue getValue() const { return {}; } void setValue(const uint8_t*, uint16_t) {} bool notify() { return true; } void setCallbacks(NimBLECharacteristicCallbacks*) {} };
class NimBLEService { public: NimBLECharacteristic* createCharacteristic(const char*, uint32_t) { return nullptr; } bool start() { return true; } };
class NimBLEServer;
class NimBLEServerCallbacks { public: virtual ~NimBLEServerCallbacks() {} virtual void onConnect(NimBLEServer*, NimBLEConnInfo&) {} virtual void onDisconnect(NimBLEServer*, NimBLEConnInfo&, int) {} };
class NimBLEServer { public: void setCallbacks(NimBLEServerCallbacks*, bool = true) {} NimBLEService* createService(const char*) { return nullptr; } bool start() { return true; } };
class NimBLEAdvertising { public: bool addServiceUUID(const char*) { return true; } };
class NimBLEDevice { public: static bool init(const std::string&) { return true; } static bool deinit(bool = false) { return true; }
  static void setSecurityPasskey(uint32_t) {} static void setSecurityIOCap(uint8_t) {} static void setSecurityAuth(bool, bool, bool) {}
  static NimBLEServer* createServer() { return nullptr; } static NimBLEAdvertising* getAdvertising() { return nullptr; } static bool startAdvertising() { return true; } static bool stopAdvertising() { return true; } };
