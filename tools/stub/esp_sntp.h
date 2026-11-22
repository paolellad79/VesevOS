#pragma once
#include <stdint.h>
#include <sys/time.h>
void sntp_set_sync_interval(uint32_t); typedef void (*sntp_sync_time_cb_t)(struct timeval*); void sntp_set_time_sync_notification_cb(sntp_sync_time_cb_t);
typedef enum { ESP_SNTP_OPMODE_POLL, ESP_SNTP_OPMODE_LISTENONLY } esp_sntp_operatingmode_t;
bool esp_sntp_enabled(void); void esp_sntp_stop(void); void esp_sntp_init(void); void esp_sntp_setoperatingmode(esp_sntp_operatingmode_t); void esp_sntp_setservername(uint8_t, const char*);
// aggiunte per vos_time.cpp (server dell'ora dal router): stessi nomi e firme di ESP-IDF 5.5.5 (components/lwip/include/apps/esp_sntp.h)
typedef uint8_t u8_t; struct ip_addr_t_stub { uint32_t addr; }; typedef struct ip_addr_t_stub ip_addr_t;
#ifndef CONFIG_LWIP_SNTP_MAX_SERVERS
#define CONFIG_LWIP_SNTP_MAX_SERVERS 3
#endif
void esp_sntp_servermode_dhcp(bool); void esp_sntp_setserver(u8_t, const ip_addr_t*); const char* esp_sntp_getservername(u8_t); const ip_addr_t* esp_sntp_getserver(u8_t);
#define ip_addr_isany(a) ((a)->addr == 0)
const char* ipaddr_ntoa(const ip_addr_t*);
