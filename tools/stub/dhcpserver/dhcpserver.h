#pragma once
// STUB ricavato da dhcpserver/dhcpserver.h (ESP-IDF 5.5)
#include <stdbool.h>
#include "esp_netif.h"
typedef struct { bool enable; esp_ip4_addr_t start_ip; esp_ip4_addr_t end_ip; } dhcps_lease_t;
