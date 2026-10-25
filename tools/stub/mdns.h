#pragma once
// STUB di mdns.h (solo quello che usa vos_drv_mdns.cpp)
#include <stdint.h>
#include <stddef.h>
#include "esp_err.h"
#include "esp_netif.h"
typedef struct mdns_txt_item_s mdns_txt_item_t;
esp_err_t mdns_init(void);
void mdns_free(void);
esp_err_t mdns_hostname_set(const char*);
esp_err_t mdns_service_add(const char*, const char*, const char*, uint16_t, mdns_txt_item_t*, size_t);
esp_err_t mdns_query_a(const char* host_name, uint32_t timeout, esp_ip4_addr_t* addr);
