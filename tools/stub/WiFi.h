#pragma once
#include <Arduino.h>
#include "esp_wifi.h"
#define WIFI_AUTH_OPEN 0
enum wl_status_t{WL_IDLE_STATUS,WL_NO_SSID_AVAIL=1,WL_CONNECTED=3,WL_CONNECT_FAILED=4,WL_DISCONNECTED=6};
#define WIFI_SCAN_RUNNING (-1)
#define WIFI_SCAN_FAILED (-2)
typedef enum { WIFI_POWER_19_5dBm=78, WIFI_POWER_2dBm=8 } wifi_power_t;
struct WiFiClass{IPAddress localIP(){return IPAddress();} IPAddress softAPIP(){return IPAddress();} IPAddress gatewayIP(){return IPAddress();} IPAddress subnetMask(){return IPAddress();} IPAddress dnsIP(int=0){return IPAddress();}
 String SSID(int=0){return "";} int32_t RSSI(int=0){return 0;} int32_t channel(int=0){return 0;} uint8_t softAPgetStationNum(){return 0;} String macAddress(){return "";} String softAPmacAddress(){return "";}
 void persistent(bool){} bool mode(wifi_mode_t){return true;} bool disconnect(bool=false,bool=false){return true;} bool softAP(const char*,const char* =nullptr,int=1,int=0,int=4,bool=false){return true;} bool softAPConfig(IPAddress,IPAddress,IPAddress){return true;}
 wl_status_t begin(const char*,const char* =nullptr,int32_t=0,const uint8_t* =nullptr,bool=true){return WL_CONNECTED;} bool config(IPAddress,IPAddress,IPAddress,IPAddress=IPAddress(),IPAddress=IPAddress()){return true;} wl_status_t status(){return WL_CONNECTED;}
 bool setHostname(const char*){return true;} bool setAutoReconnect(bool){return true;} int16_t scanNetworks(bool=false,bool=false,bool=false,uint32_t=300,uint8_t=0){return 0;} int16_t scanComplete(){return 0;} void scanDelete(){} uint8_t encryptionType(int){return 0;} bool softAPdisconnect(bool=false){return true;} bool setSleep(bool){return true;} bool setTxPower(wifi_power_t){return true;} wifi_mode_t getMode(){return WIFI_MODE_STA;} bool enableSTA(bool){return true;} bool reconnect(){return true;} uint8_t* BSSID(int=0){return nullptr;}};
#define WIFI_OFF WIFI_MODE_NULL
#define WIFI_STA WIFI_MODE_STA
#define WIFI_AP WIFI_MODE_AP
#define WIFI_AP_STA WIFI_MODE_APSTA
extern WiFiClass WiFi;
