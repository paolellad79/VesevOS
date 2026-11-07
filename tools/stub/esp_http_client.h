// Stub: solo le dichiarazioni usate da drivers/vos_drv_https.cpp, per il controllo della sintassi.
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
typedef struct esp_http_client* esp_http_client_handle_t;
typedef struct {
  const char* url;
  esp_err_t (*crt_bundle_attach)(void*);
  int timeout_ms, buffer_size, buffer_size_tx;
  bool disable_auto_redirect, keep_alive_enable;
} esp_http_client_config_t;
esp_http_client_handle_t esp_http_client_init(const esp_http_client_config_t*);
esp_err_t esp_http_client_open(esp_http_client_handle_t, int);
int64_t esp_http_client_fetch_headers(esp_http_client_handle_t);
int esp_http_client_get_status_code(esp_http_client_handle_t);
int esp_http_client_read(esp_http_client_handle_t, char*, int);
bool esp_http_client_is_complete_data_received(esp_http_client_handle_t);
esp_err_t esp_http_client_close(esp_http_client_handle_t);
esp_err_t esp_http_client_cleanup(esp_http_client_handle_t);
