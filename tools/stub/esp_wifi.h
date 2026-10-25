#pragma once
// STUB ricavato da esp_wifi.h / esp_wifi_types (ESP-IDF 5.5)
#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
typedef enum { WIFI_MODE_NULL=0, WIFI_MODE_STA, WIFI_MODE_AP, WIFI_MODE_APSTA, WIFI_MODE_MAX } wifi_mode_t;
typedef enum { WIFI_IF_STA=0, WIFI_IF_AP=1 } wifi_interface_t;
typedef enum { WIFI_SECOND_CHAN_NONE=0, WIFI_SECOND_CHAN_ABOVE, WIFI_SECOND_CHAN_BELOW } wifi_second_chan_t;
typedef enum { WIFI_COUNTRY_POLICY_AUTO, WIFI_COUNTRY_POLICY_MANUAL } wifi_country_policy_t;
typedef struct { char cc[3]; uint8_t schan; uint8_t nchan; int8_t max_tx_power; wifi_country_policy_t policy; } wifi_country_t;
typedef struct { signed rssi:8; unsigned rate:5; unsigned channel:4; } wifi_pkt_rx_ctrl_t;
typedef struct { uint8_t* des_addr; uint8_t* src_addr; wifi_interface_t ifidx; void* tx_status; } wifi_tx_info_t;
esp_err_t esp_wifi_set_country_code(const char* country, bool ieee80211d_enabled);
esp_err_t esp_wifi_get_country_code(char* country);
esp_err_t esp_wifi_set_country(const wifi_country_t* country);
esp_err_t esp_wifi_get_country(wifi_country_t* country);
esp_err_t esp_wifi_set_max_tx_power(int8_t power);
esp_err_t esp_wifi_get_max_tx_power(int8_t* power);
esp_err_t esp_wifi_set_channel(uint8_t primary, wifi_second_chan_t second);
esp_err_t esp_wifi_get_channel(uint8_t* primary, wifi_second_chan_t* second);
esp_err_t esp_wifi_set_promiscuous(bool en);
typedef enum { WIFI_PS_NONE, WIFI_PS_MIN_MODEM, WIFI_PS_MAX_MODEM } wifi_ps_type_t;
esp_err_t esp_wifi_set_ps(wifi_ps_type_t type);
// --- configurazione, eventi, scansione (firme di ESP-IDF 5.5) ---
typedef enum { WIFI_AUTH_OPEN = 0, WIFI_AUTH_WEP, WIFI_AUTH_WPA_PSK, WIFI_AUTH_WPA2_PSK } wifi_auth_mode_t;
typedef enum { WIFI_CIPHER_TYPE_NONE = 0, WIFI_CIPHER_TYPE_TKIP, WIFI_CIPHER_TYPE_CCMP } wifi_cipher_type_t;
typedef enum { WIFI_FAST_SCAN = 0, WIFI_ALL_CHANNEL_SCAN } wifi_scan_method_t;
typedef enum { WIFI_CONNECT_AP_BY_SIGNAL = 0, WIFI_CONNECT_AP_BY_SECURITY } wifi_sort_method_t;
typedef enum { WIFI_SCAN_TYPE_ACTIVE = 0, WIFI_SCAN_TYPE_PASSIVE } wifi_scan_type_t;
typedef enum { WIFI_STORAGE_FLASH, WIFI_STORAGE_RAM } wifi_storage_t;
typedef struct { bool capable; bool required; } wifi_pmf_config_t;
typedef struct { uint8_t ssid[32]; uint8_t password[64]; wifi_scan_method_t scan_method; wifi_sort_method_t sort_method;
                 struct { int8_t rssi; wifi_auth_mode_t authmode; } threshold; wifi_pmf_config_t pmf_cfg; } wifi_sta_config_t;
typedef struct { uint8_t ssid[32]; uint8_t password[64]; uint8_t ssid_len; uint8_t channel; wifi_auth_mode_t authmode; uint8_t ssid_hidden;
                 uint8_t max_connection; uint16_t beacon_interval; wifi_cipher_type_t pairwise_cipher; } wifi_ap_config_t;
typedef union { wifi_sta_config_t sta; wifi_ap_config_t ap; } wifi_config_t;
typedef struct { int static_tx_buf_num, dynamic_tx_buf_num, tx_buf_type, cache_tx_buf_num, static_rx_buf_num, dynamic_rx_buf_num; } wifi_init_config_t;
typedef struct { uint8_t bssid[6]; uint8_t ssid[33]; uint8_t primary; int8_t rssi; wifi_auth_mode_t authmode; } wifi_ap_record_t;
typedef struct { uint8_t* ssid; uint8_t* bssid; uint8_t channel; bool show_hidden; wifi_scan_type_t scan_type;
                 union { struct { uint32_t min; uint32_t max; } active; uint32_t passive; } scan_time; } wifi_scan_config_t;
typedef struct { uint8_t reason; } wifi_event_sta_disconnected_t;
typedef struct { uint8_t pin_code[8]; } wifi_event_sta_wps_er_pin_t;
typedef struct { int num; } wifi_sta_list_t;
typedef enum { WIFI_EVENT_STA_START, WIFI_EVENT_STA_STOP, WIFI_EVENT_STA_CONNECTED, WIFI_EVENT_STA_DISCONNECTED, WIFI_EVENT_AP_START, WIFI_EVENT_AP_STOP,
               WIFI_EVENT_STA_WPS_ER_SUCCESS, WIFI_EVENT_STA_WPS_ER_FAILED, WIFI_EVENT_STA_WPS_ER_TIMEOUT, WIFI_EVENT_STA_WPS_ER_PIN } wifi_event_t;
enum { WIFI_REASON_UNSPECIFIED = 1, WIFI_REASON_AUTH_EXPIRE = 2, WIFI_REASON_AUTH_LEAVE = 3, WIFI_REASON_ASSOC_EXPIRE = 4, WIFI_REASON_ASSOC_TOOMANY = 5,
       WIFI_REASON_NOT_AUTHED = 6, WIFI_REASON_NOT_ASSOCED = 7, WIFI_REASON_ASSOC_LEAVE = 8, WIFI_REASON_ASSOC_NOT_AUTHED = 9,
       WIFI_REASON_IE_IN_4WAY_DIFFERS = 17, WIFI_REASON_GROUP_KEY_UPDATE_TIMEOUT = 16, WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT = 15,
       WIFI_REASON_MIC_FAILURE = 14, WIFI_REASON_802_1X_AUTH_FAILED = 23, WIFI_REASON_BEACON_TIMEOUT = 200, WIFI_REASON_NO_AP_FOUND = 201,
       WIFI_REASON_HANDSHAKE_TIMEOUT = 204, WIFI_REASON_CONNECTION_FAIL = 205, WIFI_REASON_AP_TSF_RESET = 206, WIFI_REASON_ROAMING = 207,
       WIFI_REASON_ASSOC_FAIL = 203, WIFI_REASON_INVALID_PMKID = 53 };
#define WIFI_INIT_CONFIG_DEFAULT() wifi_init_config_t{0}
esp_err_t esp_wifi_init(const wifi_init_config_t* config);
esp_err_t esp_wifi_deinit(void);
esp_err_t esp_wifi_set_storage(wifi_storage_t storage);
esp_err_t esp_wifi_set_mode(wifi_mode_t mode);
esp_err_t esp_wifi_get_mode(wifi_mode_t* mode);
esp_err_t esp_wifi_start(void);
esp_err_t esp_wifi_stop(void);
esp_err_t esp_wifi_connect(void);
esp_err_t esp_wifi_disconnect(void);
esp_err_t esp_wifi_set_config(wifi_interface_t interface, wifi_config_t* conf);
esp_err_t esp_wifi_get_config(wifi_interface_t interface, wifi_config_t* conf);
esp_err_t esp_wifi_sta_get_ap_info(wifi_ap_record_t* ap_info);
esp_err_t esp_wifi_ap_get_sta_list(wifi_sta_list_t* sta);
esp_err_t esp_wifi_scan_start(const wifi_scan_config_t* config, bool block);
esp_err_t esp_wifi_scan_get_ap_num(uint16_t* number);
esp_err_t esp_wifi_scan_get_ap_records(uint16_t* number, wifi_ap_record_t* ap_records);
esp_err_t esp_wifi_clear_ap_list(void);
