// stub: solo per controllare la sintassi (le firme sono quelle di ESP-IDF 5.x esp_netif.h)
#pragma once
#include <stdint.h>
#include <stddef.h>
#include "esp_err.h"
typedef struct esp_netif_obj esp_netif_t;
typedef enum { ESP_NETIF_DHCP_INIT = 0, ESP_NETIF_DHCP_STARTED, ESP_NETIF_DHCP_STOPPED, ESP_NETIF_DHCP_STATUS_MAX } esp_netif_dhcp_status_t;
typedef enum { ESP_NETIF_OP_START = 0, ESP_NETIF_OP_SET, ESP_NETIF_OP_GET, ESP_NETIF_OP_MAX } esp_netif_dhcp_option_mode_t;
typedef enum { ESP_NETIF_SUBNET_MASK = 1, ESP_NETIF_DOMAIN_NAME_SERVER = 6, ESP_NETIF_ROUTER_SOLICITATION_ADDRESS = 32, ESP_NETIF_REQUESTED_IP_ADDRESS = 50, ESP_NETIF_IP_ADDRESS_LEASE_TIME = 51 } esp_netif_dhcp_option_id_t;
esp_netif_t* esp_netif_get_handle_from_ifkey(const char* if_key);
esp_err_t esp_netif_dhcps_get_status(esp_netif_t* esp_netif, esp_netif_dhcp_status_t* status);
esp_err_t esp_netif_dhcps_start(esp_netif_t* esp_netif);
esp_err_t esp_netif_dhcps_stop(esp_netif_t* esp_netif);
esp_err_t esp_netif_dhcps_option(esp_netif_t* esp_netif, esp_netif_dhcp_option_mode_t opt_op, esp_netif_dhcp_option_id_t opt_id, void* opt_val, uint32_t opt_len);

// --- per il driver Wi-Fi ---
typedef struct { uint32_t addr; } esp_ip4_addr_t;
typedef struct { esp_ip4_addr_t ip; esp_ip4_addr_t netmask; esp_ip4_addr_t gw; } esp_netif_ip_info_t;
typedef enum { ESP_IPADDR_TYPE_V4 = 0 } esp_ip_addr_type_t;
typedef struct { esp_ip_addr_type_t type; union { esp_ip4_addr_t ip4; } u_addr; } esp_ip_addr_t;
typedef struct { esp_ip_addr_t ip; } esp_netif_dns_info_t;
typedef enum { ESP_NETIF_DNS_MAIN = 0, ESP_NETIF_DNS_BACKUP, ESP_NETIF_DNS_FALLBACK, ESP_NETIF_DNS_MAX } esp_netif_dns_type_t;
esp_err_t esp_netif_init(void);
esp_netif_t* esp_netif_create_default_wifi_ap(void);
esp_netif_t* esp_netif_create_default_wifi_sta(void);
void esp_netif_destroy_default_wifi(void* esp_netif);
esp_err_t esp_netif_set_hostname(esp_netif_t* esp_netif, const char* hostname);
esp_err_t esp_netif_dhcpc_start(esp_netif_t* esp_netif);
esp_err_t esp_netif_dhcpc_stop(esp_netif_t* esp_netif);
esp_err_t esp_netif_set_ip_info(esp_netif_t* esp_netif, const esp_netif_ip_info_t* ip_info);
esp_err_t esp_netif_get_ip_info(esp_netif_t* esp_netif, esp_netif_ip_info_t* ip_info);
esp_err_t esp_netif_set_dns_info(esp_netif_t* esp_netif, esp_netif_dns_type_t type, esp_netif_dns_info_t* dns);
esp_err_t esp_netif_get_dns_info(esp_netif_t* esp_netif, esp_netif_dns_type_t type, esp_netif_dns_info_t* dns);
