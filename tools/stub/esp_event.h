#pragma once
// STUB ricavato da esp_event.h (ESP-IDF 5.5)
#include <stdint.h>
#include "esp_err.h"
typedef const char* esp_event_base_t;
typedef void* esp_event_handler_instance_t;
typedef void (*esp_event_handler_t)(void* arg, esp_event_base_t base, int32_t id, void* data);
extern esp_event_base_t WIFI_EVENT;
extern esp_event_base_t IP_EVENT;
#define ESP_EVENT_ANY_ID -1
typedef enum { IP_EVENT_STA_GOT_IP, IP_EVENT_STA_LOST_IP } ip_event_t;
esp_err_t esp_event_loop_create_default(void);
esp_err_t esp_event_handler_register(esp_event_base_t base, int32_t id, esp_event_handler_t handler, void* arg);
esp_err_t esp_event_handler_instance_register(esp_event_base_t base, int32_t id, esp_event_handler_t handler, void* arg, esp_event_handler_instance_t* inst);
esp_err_t esp_event_handler_instance_unregister(esp_event_base_t base, int32_t id, esp_event_handler_instance_t inst);
