#pragma once
#include <stdint.h>
#include <sys/time.h>
void sntp_set_sync_interval(uint32_t); typedef void (*sntp_sync_time_cb_t)(struct timeval*); void sntp_set_time_sync_notification_cb(sntp_sync_time_cb_t);
typedef enum { ESP_SNTP_OPMODE_POLL, ESP_SNTP_OPMODE_LISTENONLY } esp_sntp_operatingmode_t;
bool esp_sntp_enabled(void); void esp_sntp_stop(void); void esp_sntp_init(void); void esp_sntp_setoperatingmode(esp_sntp_operatingmode_t); void esp_sntp_setservername(uint8_t, const char*);
