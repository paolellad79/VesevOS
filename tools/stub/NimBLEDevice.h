// stub minimo esp-nimble-cpp 2.x (solo per controllo sintassi). Le firme seguono quelle vere di
// idf/managed_components/h2zero__esp-nimble-cpp/src (NimBLEServer.h, NimBLEService.h, NimBLECharacteristic.h, NimBLEAdvertising.h, NimBLEDevice.h, NimBLEConnInfo.h).
#pragma once
#include <Arduino.h>
#include <string>
#define BLE_HS_IO_DISPLAY_ONLY 0
#define BLE_HS_CONN_HANDLE_NONE 0xFFFF
typedef enum { READ = 2, READ_ENC = 4, READ_AUTHEN = 8, WRITE = 16, WRITE_ENC = 32, WRITE_AUTHEN = 64, NOTIFY = 128, INDICATE = 256 } NIMBLE_PROPERTY;
class NimBLEConnInfo {
 public:
  uint16_t getConnHandle() const { return 0; }
  bool isEncrypted() const { return false; }
  bool isAuthenticated() const { return false; }
};
class NimBLEAttValue { public: const uint8_t* data() const { return nullptr; } uint16_t length() const { return 0; } };
class NimBLECharacteristic;
class NimBLECharacteristicCallbacks { public: virtual ~NimBLECharacteristicCallbacks() {}
  virtual void onRead(NimBLECharacteristic*, NimBLEConnInfo&) {}
  virtual void onWrite(NimBLECharacteristic*, NimBLEConnInfo&) {}
  virtual void onSubscribe(NimBLECharacteristic*, NimBLEConnInfo&, uint16_t) {} };
class NimBLECharacteristic { public: NimBLEAttValue getValue() const { return {}; } void setValue(const uint8_t*, size_t) {}
  bool notify(const uint8_t*, size_t, uint16_t = BLE_HS_CONN_HANDLE_NONE) const { return true; } void setCallbacks(NimBLECharacteristicCallbacks*) {} };
class NimBLEService { public: NimBLECharacteristic* createCharacteristic(const char*, uint32_t = 0, uint16_t = 512) { return nullptr; } };
class NimBLEServer;
class NimBLEServerCallbacks { public: virtual ~NimBLEServerCallbacks() {}
  virtual void onConnect(NimBLEServer*, NimBLEConnInfo&) {}
  virtual void onDisconnect(NimBLEServer*, NimBLEConnInfo&, int) {}
  virtual void onAuthenticationComplete(NimBLEConnInfo&) {} };
class NimBLEServer { public: void setCallbacks(NimBLEServerCallbacks*, bool = true) {} NimBLEService* createService(const char*) { return nullptr; } bool start() { return true; }
  bool disconnect(uint16_t, uint8_t = 0x13) const { return true; } bool disconnect(const NimBLEConnInfo&, uint8_t = 0x13) const { return true; }
  void advertiseOnDisconnect(bool) {} uint16_t getPeerMTU(uint16_t) const { return 23; } };
class NimBLEAdvertising { public: bool addServiceUUID(const char*) { return true; } void setMinInterval(uint16_t) {} void setMaxInterval(uint16_t) {}
  void enableScanResponse(bool) {} bool setName(const std::string&) { return true; } };
class NimBLEDevice { public: static bool init(const std::string&) { return true; } static bool deinit(bool = false) { return true; }
  static void setSecurityPasskey(uint32_t) {} static void setSecurityIOCap(uint8_t) {} static void setSecurityAuth(bool, bool, bool) {}
  static bool startSecurity(uint16_t, int* = nullptr) { return true; }
  static NimBLEServer* createServer() { return nullptr; } static NimBLEAdvertising* getAdvertising() { return nullptr; }
  static bool startAdvertising(uint32_t = 0) { return true; } static bool stopAdvertising() { return true; } };
