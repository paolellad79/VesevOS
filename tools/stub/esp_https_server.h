#pragma once
#include "esp_http_server.h"
typedef struct httpd_ssl_config { httpd_config_t httpd; const uint8_t* servercert; size_t servercert_len; const uint8_t* cacert_pem; size_t cacert_len; const uint8_t* prvtkey_pem; size_t prvtkey_len; int transport_mode; uint16_t port_secure; uint16_t port_insecure; } httpd_ssl_config_t;
#define HTTPD_SSL_CONFIG_DEFAULT() {}
