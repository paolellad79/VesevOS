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
