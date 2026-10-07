#pragma once
#define ESP_LOGE(t,...) do{}while(0)
#define ESP_LOGW(t,...) do{}while(0)
#define ESP_LOGI(t,...) do{}while(0)
#define ESP_LOGD(t,...) do{}while(0)
#define ESP_LOGV(t,...) do{}while(0)
typedef enum { ESP_LOG_NONE, ESP_LOG_ERROR, ESP_LOG_WARN, ESP_LOG_INFO, ESP_LOG_DEBUG, ESP_LOG_VERBOSE } esp_log_level_t;
void esp_log_level_set(const char* tag, esp_log_level_t level);
